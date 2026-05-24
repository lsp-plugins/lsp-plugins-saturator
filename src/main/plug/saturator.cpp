/*
 * Copyright (C) 2026 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2026 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-saturator
 * Created on: 19 Apr 2026 г.
 *
 * lsp-plugins-saturator is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-saturator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-saturator. If not, see <https://www.gnu.org/licenses/>.
 */

#include <lsp-plug.in/common/alloc.h>
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/dsp/dsp.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/plug-fw/meta/func.h>
#include <lsp-plug.in/shared/debug.h>

#include <private/plugins/saturator.h>

namespace lsp
{
    namespace plugins
    {
        /* The size of temporary buffer for audio processing */
        static constexpr size_t BUFFER_SIZE             = 0x200;
        /* The FIR rank for the EQ */
        constexpr static size_t EQ_RANK                 = 12;

        //---------------------------------------------------------------------
        // Plugin factory
        static const meta::plugin_t *plugins[] =
        {
            &meta::saturator_x3_mono,
            &meta::saturator_x3_stereo,
            &meta::saturator_x8_mono,
            &meta::saturator_x8_stereo,
            &meta::saturator_x16_mono,
            &meta::saturator_x16_stereo,
            &meta::saturator_x32_mono,
            &meta::saturator_x32_stereo,
        };

        typedef struct plugin_settings_t
        {
            const meta::plugin_t   *metadata;
            uint8_t                 bands;
        } plugin_settings_t;

        static const plugin_settings_t plugin_settings[] =
        {
            {&meta::saturator_x3_mono,      3   },
            {&meta::saturator_x3_stereo,    3   },
            {&meta::saturator_x8_mono,      8   },
            {&meta::saturator_x8_stereo,    8   },
            {&meta::saturator_x16_mono,     16  },
            {&meta::saturator_x16_stereo,   16  },
            {&meta::saturator_x32_mono,     32  },
            {&meta::saturator_x32_stereo,   32  },

            { NULL, 0 }
        };

        static plug::Module *plugin_factory(const meta::plugin_t *meta)
        {
            for (const plugin_settings_t *s = plugin_settings; s->metadata != NULL; ++s)
                if (s->metadata == meta)
                    return new saturator(s->metadata, s->bands);

            return NULL;
        }

        static plug::Factory factory(plugin_factory, plugins, 2);

        //---------------------------------------------------------------------
        // Implementation
        saturator::saturator(const meta::plugin_t *meta, size_t bands):
            Module(meta)
        {
            // Compute the number of audio channels by the number of inputs
            nChannels = 0;
            for (const meta::port_t *p = meta->ports; p->id != NULL; ++p)
                if (meta::is_audio_in_port(p))
                    ++nChannels;

            // Initialize other parameters
            nSampleRate     = 0;
            nBands          = bands;
            vChannels       = NULL;
            vBuffer         = NULL;

            pBypass         = NULL;
            pComment        = NULL;

            pData           = NULL;
        }

        saturator::~saturator()
        {
            do_destroy();
        }

        void saturator::init(plug::IWrapper *wrapper, plug::IPort **ports)
        {
            // Call parent class for initialization
            Module::init(wrapper, ports);

            // Calculate the number of bytes to allocate
            size_t szof_filter_arr  = align_size(sizeof(eq_band_t) * nBands, OPTIMAL_ALIGN);
            size_t szof_filters     = align_size(nChannels * 2 * szof_filter_arr, OPTIMAL_ALIGN);
            size_t szof_channels    = align_size(sizeof(channel_t) * nChannels, OPTIMAL_ALIGN);
            size_t buf_sz           = BUFFER_SIZE * sizeof(float);
            size_t alloc            = szof_channels + szof_filters + buf_sz;

            // Allocate memory-aligned data
            uint8_t *ptr            = alloc_aligned<uint8_t>(pData, alloc, OPTIMAL_ALIGN);
            if (ptr == NULL)
                return;

            // Assign all resources
            vChannels               = advance_ptr_bytes<channel_t>(ptr, szof_channels);

            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t *c = &vChannels[i];

                c->vPreEQBands    = advance_ptr_bytes<eq_band_t>(ptr, szof_filter_arr);
                c->vPostEQBands   = advance_ptr_bytes<eq_band_t>(ptr, szof_filter_arr);

                // Construct in-place DSP processors
                c->sBypass.construct();
                c->sPreEQ.construct();
                c->sOversampler.construct();
                c->sShaper.construct();
                c->sPostEQ.construct();

                c->sPreEQ.init(nBands, meta::saturator::FFT_RANK);
                c->sPostEQ.init(nBands, meta::saturator::FFT_RANK);

                c->pIn                  = NULL;
                c->pOut                 = NULL;
            }

            vBuffer                 = advance_ptr_bytes<float>(ptr, buf_sz);

            // Bind ports
            lsp_trace("Binding ports");
            size_t port_id = 0;

            // Bind input audio ports
            lsp_trace("Binding input audio ports");
            for (size_t i=0; i<nChannels; ++i)
                BIND_PORT(vChannels[i].pIn);

            // Bind output audio ports
            lsp_trace("Binding output audio ports");
            for (size_t i=0; i<nChannels; ++i)
                BIND_PORT(vChannels[i].pOut);

            // Bind common ports
            lsp_trace("Binding common ports");
            BIND_PORT(pBypass);

            // Bind saturator control ports
            lsp_trace("Binding saturator control ports");
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c = &vChannels[i];

                BIND_PORT(c->sOversamplerParams.pMode);
                BIND_PORT(c->sShaperParams.pPreGain);
                BIND_PORT(c->sShaperParams.pPostGain);
                BIND_PORT(c->sShaperParams.pSlope);
                BIND_PORT(c->sShaperParams.pShape);
                BIND_PORT(c->sShaperParams.pHighLevel);
                BIND_PORT(c->sShaperParams.pLowLevel);
                BIND_PORT(c->sShaperParams.pRadius);
                BIND_PORT(c->sShaperParams.pLevels);
                BIND_PORT(c->sShaperParams.pCCompanding);
                BIND_PORT(c->sShaperParams.pQCompanding);
                BIND_PORT(c->sShaperParams.pBias);
                BIND_PORT(c->sShaperParams.pDrive);
                BIND_PORT(c->sShaperParams.pBlend);
                BIND_PORT(c->sShaperParams.pShapingFcn);

                if (i > 0)
                {
                    //channel_t *pc = &vChannels[0];

                    // TODO: Ports shared between channels go here
                }
                else
                {
                    // TODO: Initialize input controls for the first channel
                }
            }

            // Bind Pre-EQ ports
            lsp_trace("Binding pre-eq filter ports");
            for (size_t i=0; i<nBands; ++i)
            {
                for (size_t j=0; j<nChannels; ++j)
                {
                    eq_band_t *b = &vChannels[j].vPreEQBands[i];

                    BIND_PORT(b->pSolo);
                    BIND_PORT(b->pMute);
                    BIND_PORT(b->pEnable);
                    BIND_PORT(b->pGain);
                }
            }

            // Bind Pre-EQ ports
            lsp_trace("Binding post-eq filter ports");
            for (size_t i=0; i<nBands; ++i)
            {
                for (size_t j=0; j<nChannels; ++j)
                {
                    eq_band_t *b = &vChannels[j].vPostEQBands[i];

                    BIND_PORT(b->pSolo);
                    BIND_PORT(b->pMute);
                    BIND_PORT(b->pEnable);
                    BIND_PORT(b->pGain);
                }
            }

            // TODO: Bind output ports.
            BIND_PORT(pComment);

            // TODO: Bind output ports
            for (size_t i=0; i<nChannels; ++i)
            {
                //channel_t *c            = &vChannels[i];

                if (i > 0)
                {
                    //channel_t *pc           = &vChannels[0];
                    // TODO: Output ports shared between channels go here
                }
                else
                {
                    // TODO: Initialize output ports for the first channel
                }
            }
        }

        void saturator::destroy()
        {
            Module::destroy();
            do_destroy();
        }

        void saturator::do_destroy()
        {
            // Destroy channels
            if (vChannels != NULL)
            {
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c    = &vChannels[i];
                    c->sBypass.destroy();
                    c->sPreEQ.destroy();
                    c->sOversampler.destroy();
                    c->sShaper.destroy();
                    c->sPostEQ.destroy();
                }
                vChannels   = NULL;
            }

            vBuffer     = NULL;

            // Free previously allocated data chunk
            if (pData != NULL)
            {
                free_aligned(pData);
                pData       = NULL;
            }
        }

        void saturator::update_sample_rate(long sr)
        {
            nSampleRate = sr;

            // Update sample rate for the bypass processors
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c = &vChannels[i];
                c->sBypass.init(nSampleRate);
                c->sPreEQ.set_sample_rate(nSampleRate);
                c->sShaper.set_sample_rate(nSampleRate);
                c->sPostEQ.set_sample_rate(nSampleRate);

                c->sOversamplerParams.nOverSampleRate = nSampleRate * c->sOversamplerParams.nOversampling;
            }
        }

        void saturator::update_settings()
        {
            bool bypass             = pBypass->value() >= 0.5f;

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                // TODO: Store the parameters for each processor

                // TODO: Update processors
                c->sBypass.set_bypass(bypass);
            }

            // Output comment to log
        #ifdef LSP_TRACE
            if (pComment != NULL)
            {
                const char *str = pComment->buffer<char>();
                if (str != NULL)
                {
                    lsp_trace("Current comment is: %s", str);
                }
            }
        #endif /* LSP_TRACE */
        }

        void saturator::process(size_t samples)
        {
            // Process each channel independently
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                // Get input and output buffers
                const float *in         = c->pIn->buffer<float>();
                float *out              = c->pOut->buffer<float>();
                if ((in == NULL) || (out == NULL))
                    continue;

                // Process the channel with BUFFER_SIZE chunks
                // Note: since input buffer pointer can be the same to output buffer pointer,
                // we need to store the processed signal data to temporary buffer before
                // it gets processed by the dspu::Bypass processor.
                for (size_t n=0; n<samples; )
                {
                    const size_t to_do      = samples - n;
                    const size_t to_do_up   = c->sOversamplerParams.nOversampling * to_do;

                    const size_t count_up   = lsp_min(to_do_up, BUFFER_SIZE);
                    const size_t count      = count_up / c->sOversamplerParams.nOversampling;

                    c->sPreEQ.process(vBuffer, in, count);
                    dsp::mul_k2(vBuffer, c->sShaperParams.fPreGain, count);
                    c->sOversampler.upsample(vBuffer, vBuffer, count);
                    c->sShaper.process_overwrite(vBuffer, vBuffer, count_up);
                    c->sOversampler.downsample(vBuffer, vBuffer, count);
                    dsp::mul_k2(vBuffer, c->sShaperParams.fPostGain, count);
                    c->sPostEQ.process(vBuffer, in, count);

                    // Process the
                    //  - dry (unprocessed) signal stored in 'in'
                    //  - wet (processed) signal stored in 'vBuffer'
                    // Output the result to 'out' buffer
                    c->sBypass.process(out, in, vBuffer, count);

                    // Increment pointers
                    in                     +=  count;
                    out                    +=  count;
                    n                      +=  count;
                }
            }
        }

        void saturator::dump(dspu::IStateDumper *v) const
        {
            plug::Module::dump(v);

            // It is very useful to dump plugin state for debug purposes
            v->write("nChannels", nChannels);
            v->begin_array("vChannels", vChannels, nChannels);
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                v->begin_object(c, sizeof(channel_t));
                {
                    v->write_object("sBypass", &c->sBypass);
                    v->write_object("sPreEQ", &c->sPreEQ);
                    v->write_object("sOversampler", &c->sOversampler);
                    v->write_object("sShaper", &c->sShaper);
                    v->write_object("sPostEQ", &c->sPostEQ);

                    // TODO: Dump the various parameters structures

                    v->write("pIn", c->pIn);
                    v->write("pOut", c->pOut);
                }
                v->end_object();
            }
            v->end_array();

            v->write("vBuffer", vBuffer);

            v->write("pBypass", pBypass);

            v->write("pData", pData);
        }

    } /* namespace plugins */
} /* namespace lsp */


