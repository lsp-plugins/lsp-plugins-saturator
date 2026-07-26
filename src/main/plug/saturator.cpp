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
            const float            *frequencies;
        } plugin_settings_t;

        static const plugin_settings_t plugin_settings[] =
        {
            {&meta::saturator_x3_mono,      3,  meta::saturator::band_frequencies_x3   },
            {&meta::saturator_x3_stereo,    3,  meta::saturator::band_frequencies_x3   },
            {&meta::saturator_x8_mono,      8,  meta::saturator::band_frequencies_x8   },
            {&meta::saturator_x8_stereo,    8,  meta::saturator::band_frequencies_x8   },
            {&meta::saturator_x16_mono,     16, meta::saturator::band_frequencies_x16  },
            {&meta::saturator_x16_stereo,   16, meta::saturator::band_frequencies_x16  },
            {&meta::saturator_x32_mono,     32, meta::saturator::band_frequencies_x32  },
            {&meta::saturator_x32_stereo,   32, meta::saturator::band_frequencies_x32  },

            { NULL, 0 }
        };

        static plug::Module *plugin_factory(const meta::plugin_t *meta)
        {
            for (const plugin_settings_t *s = plugin_settings; s->metadata != NULL; ++s)
                if (s->metadata == meta)
                    return new saturator(s->metadata, s->bands, s->frequencies);

            return NULL;
        }

        static plug::Factory factory(plugin_factory, plugins, 2);

        //---------------------------------------------------------------------
        // Implementation
        saturator::saturator(const meta::plugin_t *meta, size_t bands, const float *vfreqs):
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
            vOSBuffer       = NULL;

            pBypass         = NULL;
            pComment        = NULL;

            vFreqs          = vfreqs;

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
            // A single filter array is an array of nBands `eq_band_t` objects:
            const size_t szof_eq_bands  = align_size(sizeof(eq_band_t) * nBands, OPTIMAL_ALIGN);
            // We have nChannels `channel_t` objects:
            const size_t szof_channels  = align_size(sizeof(channel_t) * nChannels, OPTIMAL_ALIGN);
            // We have 1x audio buffer for processing:
            const size_t szof_buf       = BUFFER_SIZE * sizeof(float);
            const size_t szof_osbuf     = szof_buf * meta::saturator::SAT_OVS_MAX;
            /** Each `ch_state_stage_t` object has:
             * 1x vfPV_pre_eq_pGain vector of nBands float
             * 1x vfPV_pre_eq_pSolo vector of nBands float
             * 1x vfPV_pre_eq_pMute vector of nBands float
             * 1x vfPV_pre_eq_pEnable vector of nBands float
             * 1x vfPV_post_eq_pGain vector of nBands float
             * 1x vfPV_post_eq_pSolo vector of nBands float
             * 1x vfPV_post_eq_pMute vector of nBands float
             * 1x vfPV_post_eq_pEnable vector of nBands float
             *
             * So, 8 arrays of nBands floats per `ch_state_stage_t` object.
             * We have a `ch_state_stage_t` object per channel. So:
             */
            const size_t szof_stage_array   = nBands * sizeof(float);

            // In total:
            const size_t alloc          =
                szof_channels +         // vChannels
                nChannels * (
                    szof_eq_bands*2 +               // vPreEQBands + vPostEQBands
                    8 * szof_stage_array            // channel_t::sStateStage buffers
                ) +
                szof_buf +              // vBuffer
                szof_osbuf;             // vOSBuffer

            // Allocate memory-aligned data
            uint8_t *ptr                = alloc_aligned<uint8_t>(pData, alloc, OPTIMAL_ALIGN);
            if (ptr == NULL)
                return;
            lsp_guard_assert(uint8_t * const save = ptr);

            // Assign all resources
            vChannels                   = advance_ptr_bytes<channel_t>(ptr, szof_channels);

            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t *c = &vChannels[i];

                c->sStateStage.vfPV_pre_eq_pGain        = advance_ptr_bytes<float>(ptr, szof_stage_array);
                c->sStateStage.vfPV_pre_eq_pSolo        = advance_ptr_bytes<float>(ptr, szof_stage_array);
                c->sStateStage.vfPV_pre_eq_pMute        = advance_ptr_bytes<float>(ptr, szof_stage_array);
                c->sStateStage.vfPV_pre_eq_pEnable      = advance_ptr_bytes<float>(ptr, szof_stage_array);

                c->sStateStage.vfPV_post_eq_pGain       = advance_ptr_bytes<float>(ptr, szof_stage_array);
                c->sStateStage.vfPV_post_eq_pSolo       = advance_ptr_bytes<float>(ptr, szof_stage_array);
                c->sStateStage.vfPV_post_eq_pMute       = advance_ptr_bytes<float>(ptr, szof_stage_array);
                c->sStateStage.vfPV_post_eq_pEnable     = advance_ptr_bytes<float>(ptr, szof_stage_array);

                // Only now we are ready to initialize the stage. Allocation must happen first.
                init_state_stage(c);

                c->vPreEQBands          = advance_ptr_bytes<eq_band_t>(ptr, szof_eq_bands);
                c->vPostEQBands         = advance_ptr_bytes<eq_band_t>(ptr, szof_eq_bands);

                // Construct in-place DSP processors
                c->sBypass.construct();
                c->sPreEQ.construct();
                c->sOversampler.construct();
                c->sShaper.construct();
                c->sPostEQ.construct();

                c->sPreEQ.init(nBands, meta::saturator::FFT_RANK);
                c->sOversampler.init();
                c->sPostEQ.init(nBands, meta::saturator::FFT_RANK);

                c->pIn                  = NULL;
                c->pOut                 = NULL;
            }

            vBuffer                 = advance_ptr_bytes<float>(ptr, szof_buf);
            vOSBuffer               = advance_ptr_bytes<float>(ptr, szof_osbuf);

            // Check that allocation did not went out of bounds. There will be an error in log otherwise
            lsp_assert(ptr <= &save[alloc]);

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


        dspu::over_mode_t saturator::get_oversampler_mode(size_t portValue)
        {
            switch (portValue)
            {
                case meta::saturator::SAT_OVS_NONE:
                    return dspu::OM_NONE;
                case meta::saturator::SAT_OVS_2X:
                    return dspu::OM_LANCZOS_2X24BIT;
                case meta::saturator::SAT_OVS_3X:
                    return dspu::OM_LANCZOS_3X24BIT;
                case meta::saturator::SAT_OVS_4X:
                    return dspu::OM_LANCZOS_4X24BIT;
                case meta::saturator::SAT_OVS_6X:
                    return dspu::OM_LANCZOS_6X24BIT;
                case meta::saturator::SAT_OVS_8X:
                default:
                    return dspu::OM_LANCZOS_8X24BIT;
            }
        }

        dspu::sh_function_t saturator::get_shaping_function(size_t portValue)
        {
            switch (portValue)
            {
                case meta::saturator::SAT_SH_FCN_SINUSOIDAL:
                    return dspu::SH_FCN_SINUSOIDAL;
                case meta::saturator::SAT_SH_FCN_POLYNOMIAL:
                    return dspu::SH_FCN_POLYNOMIAL;
                case meta::saturator::SAT_SH_FCN_HYPERBOLIC:
                    return dspu::SH_FCN_HYPERBOLIC;
                case meta::saturator::SAT_SH_FCN_EXPONENTIAL:
                    return dspu::SH_FCN_EXPONENTIAL;
                case meta::saturator::SAT_SH_FCN_POWER:
                    return dspu::SH_FCN_POWER;
                case meta::saturator::SAT_SH_FCN_BILINEAR:
                    return dspu::SH_FCN_BILINEAR;
                case meta::saturator::SAT_SH_FCN_ASYMMETRIC_CLIP:
                    return dspu::SH_FCN_ASYMMETRIC_CLIP;
                case meta::saturator::SAT_SH_FCN_ASYMMETRIC_SOFTCLIP:
                    return dspu::SH_FCN_ASYMMETRIC_SOFTCLIP;
                case meta::saturator::SAT_SH_FCN_QUARTER_CIRCLE:
                    return dspu::SH_FCN_QUARTER_CIRCLE;
                case meta::saturator::SAT_SH_FCN_RECTIFIER:
                    return dspu::SH_FCN_RECTIFIER;
                case meta::saturator::SAT_SH_FCN_BITCRUSH_FLOOR:
                    return dspu::SH_FCN_BITCRUSH_FLOOR;
                case meta::saturator::SAT_SH_FCN_BITCRUSH_CEIL:
                    return dspu::SH_FCN_BITCRUSH_CEIL;
                case meta::saturator::SAT_SH_FCN_BITCRUSH_ROUND:
                    return dspu::SH_FCN_BITCRUSH_ROUND;
                case meta::saturator::SAT_SH_FCN_CONTINUOUS_A_LAW_COMPRESSION:
                    return dspu::SH_FCN_CONTINUOUS_A_LAW_COMPRESSION;
                case meta::saturator::SAT_SH_FCN_CONTINUOUS_A_LAW_EXPANSION:
                    return dspu::SH_FCN_CONTINUOUS_A_LAW_EXPANSION;
                case meta::saturator::SAT_SH_FCN_CONTINUOUS_MU_LAW_COMPRESSION:
                    return dspu::SH_FCN_CONTINUOUS_MU_LAW_COMPRESSION;
                case meta::saturator::SAT_SH_FCN_CONTINUOUS_MU_LAW_EXPANSION:
                    return dspu::SH_FCN_CONTINUOUS_MU_LAW_EXPANSION;
                case meta::saturator::SAT_SH_FCN_QUANTIZED_A_LAW_COMPRESSION:
                    return dspu::SH_FCN_QUANTIZED_A_LAW_COMPRESSION;
                case meta::saturator::SAT_SH_FCN_QUANTIZED_A_LAW_EXPANSION:
                    return dspu::SH_FCN_QUANTIZED_A_LAW_EXPANSION;
                case meta::saturator::SAT_SH_FCN_QUANTIZED_MU_LAW_COMPRESSION:
                    return dspu::SH_FCN_QUANTIZED_MU_LAW_COMPRESSION;
                case meta::saturator::SAT_SH_FCN_QUANTIZED_MU_LAW_EXPANSION:
                    return dspu::SH_FCN_QUANTIZED_MU_LAW_EXPANSION;
                case meta::saturator::SAT_SH_FCN_TAP_TUBEWARMTH:
                    return dspu::SH_FCN_TAP_TUBEWARMTH;
                default:
                    return dspu::SH_FCN_DEFAULT;
            }
        }

        void saturator::init_state_stage(channel_t *c)
        {
            c->nUpdate = 0;

            for (size_t b=0; b<nBands; ++b)
            {
                c->sStateStage.vfPV_pre_eq_pGain[b]     = meta::saturator::BAND_GAIN_DFL;
                c->sStateStage.vfPV_pre_eq_pSolo[b]     = 0.0f; // TODO: Should we put something in meta for this?
                c->sStateStage.vfPV_pre_eq_pMute[b]     = 0.0f; // TODO: Should we put something in meta for this?
                c->sStateStage.vfPV_pre_eq_pEnable[b]   = 0.0f; // TODO: Should we put something in meta for this?

                c->sStateStage.vfPV_post_eq_pGain[b]    = meta::saturator::BAND_GAIN_DFL;
                c->sStateStage.vfPV_post_eq_pSolo[b]    = 0.0f; // TODO: Should we put something in meta for this?
                c->sStateStage.vfPV_post_eq_pMute[b]    = 0.0f; // TODO: Should we put something in meta for this?
                c->sStateStage.vfPV_post_eq_pEnable[b]  = 0.0f; // TODO: Should we put something in meta for this?
            }
            c->nUpdate |= UPD_PRE_EQ | UPD_POST_EQ;

            c->sStateStage.nPV_ovs_pMode = meta::saturator::SAT_OVS_DFL;
            c->nUpdate |= UPD_OVERSAMPLER;

            c->sStateStage.fPV_shaper_pPreGain      = meta::saturator::PRE_GAIN_DFL;
            c->sStateStage.fPV_shaper_pPostGain     = meta::saturator::POST_GAIN_DFL;
            c->sStateStage.fPV_shaper_pSlope        = meta::saturator::SLOPE_DFL;
            c->sStateStage.fPV_shaper_pShape        = meta::saturator::SHAPE_DFL;
            c->sStateStage.fPV_shaper_pHighLevel    = meta::saturator::HIGH_LEVEL_DFL;
            c->sStateStage.fPV_shaper_pLowLevel     = meta::saturator::LOW_LEVEL_DFL;
            c->sStateStage.fPV_shaper_pRadius       = meta::saturator::RADIUS_DFL;
            c->sStateStage.fPV_shaper_pLevels       = meta::saturator::LEVELS_DFL;
            c->sStateStage.fPV_shaper_pCCompanding  = meta::saturator::C_COMPANDING_DFL;
            c->sStateStage.fPV_shaper_pQCompanding  = meta::saturator::Q_COMPANDING_DFL;
            c->sStateStage.fPV_shaper_pBias         = meta::saturator::BIAS_DFL;
            c->sStateStage.fPV_shaper_pDrive        = meta::saturator::DRIVE_DFL;
            c->sStateStage.fPV_shaper_pBlend        = meta::saturator::BLEND_DFL;
            c->sStateStage.nPV_shaper_pShapingFcn   = meta::saturator::SAT_SH_FCN_DEFAULT;
            c->nUpdate |= UPD_SHAPER;

        }

        void saturator::setup_eq_filter(dspu::filter_params_t *fp, size_t band, float gain, bool mute, bool enable, bool has_solo, bool solo_band)
        {
            // TODO: As we add more settings for the filter bands we will need to to change this,

            fp->nSlope      = 1;
            fp->fQuality    = 0.0f;
            fp->fGain       = gain;

            if (band == 0)
            {
                fp->nType   = dspu::FLT_DR_APO_LOPASS;
                fp->fFreq   = sqrtf(vFreqs[0] * vFreqs[1]);
                fp->fFreq2  = fp->fFreq;
            }
            else if (band == (nBands - 1))
            {
                fp->nType   = dspu::FLT_DR_APO_HIPASS;
                fp->fFreq   = sqrtf(vFreqs[band-1] * vFreqs[band]);
                fp->fFreq2  = fp->fFreq;
            }
            else
            {
                fp->nType   = dspu::FLT_DR_APO_BANDPASS;
                fp->fFreq   = sqrtf(vFreqs[band-1] * vFreqs[band]);
                fp->fFreq2  = sqrtf(vFreqs[band] * vFreqs[band+1]);
            }

            if (has_solo && (band != solo_band))
            {
                fp->nType   = dspu::FLT_NONE;
                fp->fGain   = meta::saturator::BAND_GAIN_MIN;
            }

            if (mute >= 0.5f)
            {
                fp->nType   = dspu::FLT_NONE;
                fp->fGain   = meta::saturator::BAND_GAIN_MIN;
            }

            if (enable < 0.5f)
            {
                fp->nType   = dspu::FLT_NONE;
                fp->fGain   = meta::saturator::BAND_GAIN_MIN;
            }
        }

        void saturator::commit_staged_state_change(channel_t *c)
        {
            if (c->nUpdate == 0)
                return;


            for (size_t b=0; b<nBands; ++b)
            {
                // TODO: This will update all filters even if only one has changed...
                // are there better ways?
                if (c->nUpdate & UPD_PRE_EQ)
                {
                    eq_band_t *pre_f                = &c->vPreEQBands[b];
                    pre_f->sOldFP                   = pre_f->sFP;
                    dspu::filter_params_t *pre_fp   = &pre_f->sFP;
//                    dspu::filter_params_t *pre_op   = &pre_f->sOldFP;

                    setup_eq_filter(
                            pre_fp,
                            b,
                            c->sStateStage.vfPV_pre_eq_pGain[b],
                            c->sStateStage.vfPV_pre_eq_pMute[b],
                            c->sStateStage.vfPV_pre_eq_pEnable[b],
                            c->bPreHasSolo,
                            c->nPreSoloBand
                    );

                    c->sPreEQ.set_params(b, pre_fp);
                }

                if (c->nUpdate & UPD_POST_EQ)
                {
                    eq_band_t *post_f               = &c->vPostEQBands[b];
                    post_f->sOldFP                  = post_f->sFP;
                    dspu::filter_params_t *post_fp  = &post_f->sFP;
 //                   dspu::filter_params_t *pre_op   = &pre_f->sOldFP;

                    setup_eq_filter(
                            post_fp,
                            b,
                            c->sStateStage.vfPV_post_eq_pGain[b],
                            c->sStateStage.vfPV_post_eq_pMute[b],
                            c->sStateStage.vfPV_post_eq_pEnable[b],
                            c->bPostHasSolo,
                            c->nPostSoloBand
                    );

                    c->sPostEQ.set_params(b, post_fp);
                }
            }

            if (c->nUpdate & UPD_OVERSAMPLER)
            {
                c->sOversamplerParams.enOverMode = get_oversampler_mode(c->sStateStage.nPV_ovs_pMode);

                c->sOversampler.set_mode(c->sOversamplerParams.enOverMode);
                if (c->sOversampler.modified())
                    c->sOversampler.update_settings();

                c->sOversamplerParams.nOversampling     = c->sOversampler.get_oversampling();
                c->sOversamplerParams.nOverSampleRate   = nSampleRate * c->sOversamplerParams.nOversampling ;
            }

            if (c->nUpdate & UPD_SHAPER)
            {
                c->sShaperParams.fPreGain = c->sStateStage.fPV_shaper_pPreGain;
                c->sShaperParams.fPostGain = c->sStateStage.fPV_shaper_pPostGain;

                c->sShaperParams.fSlope = c->sStateStage.fPV_shaper_pSlope;
                c->sShaper.set_slope(c->sShaperParams.fSlope);

                c->sShaperParams.fShape = c->sStateStage.fPV_shaper_pShape;
                c->sShaper.set_shape(c->sShaperParams.fShape);

                c->sShaperParams.fHighLevel = c->sStateStage.fPV_shaper_pHighLevel;
                c->sShaper.set_high_level(c->sShaperParams.fHighLevel);

                c->sShaperParams.fLowLevel = c->sStateStage.fPV_shaper_pLowLevel;
                c->sShaper.set_low_level(c->sShaperParams.fLowLevel);

                c->sShaperParams.fRadius = c->sStateStage.fPV_shaper_pRadius;
                c->sShaper.set_radius(c->sShaperParams.fRadius);

                c->sShaperParams.fLevels = c->sStateStage.fPV_shaper_pLevels;
                c->sShaper.set_levels(c->sShaperParams.fLevels);

                c->sShaperParams.fCCompanding = c->sStateStage.fPV_shaper_pCCompanding;
                c->sShaper.set_continuous_companding(c->sShaperParams.fCCompanding);

                c->sShaperParams.fQCompanding = c->sStateStage.fPV_shaper_pQCompanding;
                c->sShaper.set_quantized_companding(c->sShaperParams.fQCompanding);

                c->sShaperParams.fBias = c->sStateStage.fPV_shaper_pBias;
                c->sShaper.set_bias(c->sShaperParams.fBias);

                c->sShaperParams.fDrive = c->sStateStage.fPV_shaper_pDrive;
                c->sShaper.set_drive(c->sShaperParams.fDrive);

                c->sShaperParams.fBlend = c->sStateStage.fPV_shaper_pBlend;
                c->sShaper.set_blend(c->sShaperParams.fBlend);

                c->sShaperParams.enShapingFcn = get_shaping_function(c->sStateStage.nPV_shaper_pShapingFcn);
                c->sShaper.set_function(c->sShaperParams.enShapingFcn);
            }

            c->nUpdate = 0;
        }

        void saturator::update_settings()
        {
            bool bypass = pBypass->value() >= 0.5f;

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c = &vChannels[i];

                c->sBypass.set_bypass(bypass);

                c->bPreHasSolo      = false;
                c->bPostHasSolo     = false;

                for (size_t b=0; b < nBands; ++b)
                {
                    float pre_gain = c->vPreEQBands[b].pGain->value();
                    if (pre_gain != c->sStateStage.vfPV_pre_eq_pGain[b]) {
                        c->sStateStage.vfPV_pre_eq_pGain[b] = pre_gain;
                        c->nUpdate |= UPD_PRE_EQ;
                    }

                    float pre_solo = c->vPreEQBands[b].pSolo->value();
                    if (pre_solo != c->sStateStage.vfPV_pre_eq_pSolo[b]) {
                        c->sStateStage.vfPV_pre_eq_pSolo[b] = pre_solo;
                        c->nUpdate |= UPD_PRE_EQ;
                    }

                    if (pre_solo >= 0.5f)
                    {
                        c->bPreHasSolo  = true;
                        c->nPreSoloBand = b;
                    }

                    float pre_mute = c->vPreEQBands[b].pMute->value();
                    if (pre_mute != c->sStateStage.vfPV_pre_eq_pMute[b]) {
                        c->sStateStage.vfPV_pre_eq_pMute[b] = pre_mute;
                        c->nUpdate |= UPD_PRE_EQ;
                    }

                    float pre_enable = c->vPreEQBands[b].pEnable->value();
                    if (pre_enable != c->sStateStage.vfPV_pre_eq_pEnable[b]) {
                        c->sStateStage.vfPV_pre_eq_pEnable[b] = pre_enable;
                        c->nUpdate |= UPD_PRE_EQ;
                    }

                    float post_gain = c->vPostEQBands[b].pGain->value();
                    if (post_gain != c->sStateStage.vfPV_post_eq_pGain[b]) {
                        c->sStateStage.vfPV_post_eq_pGain[b] = post_gain;
                        c->nUpdate |= UPD_POST_EQ;
                    }

                    float post_solo = c->vPostEQBands[b].pSolo->value();
                    if (post_solo != c->sStateStage.vfPV_post_eq_pSolo[b]) {
                        c->sStateStage.vfPV_post_eq_pSolo[b] = post_solo;
                        c->nUpdate |= UPD_POST_EQ;
                    }

                    if (post_solo >= 0.5f)
                    {
                        c->bPostHasSolo  = true;
                        c->nPostSoloBand = b;
                    }

                    float post_mute = c->vPostEQBands[b].pMute->value();
                    if (post_mute != c->sStateStage.vfPV_post_eq_pMute[b]) {
                        c->sStateStage.vfPV_post_eq_pMute[b] = post_mute;
                        c->nUpdate |= UPD_POST_EQ;
                    }

                    float post_enable = c->vPostEQBands[b].pEnable->value();
                    if (post_enable != c->sStateStage.vfPV_post_eq_pEnable[b]) {
                        c->sStateStage.vfPV_post_eq_pEnable[b] = post_enable;
                        c->nUpdate |= UPD_POST_EQ;
                    }
                }

                size_t overmode = c->sOversamplerParams.pMode->value();
                if (overmode != c->sStateStage.nPV_ovs_pMode)
                {
                    c->sStateStage.nPV_ovs_pMode = overmode;
                    c->nUpdate |= UPD_OVERSAMPLER;
                }

                float pregain = c->sShaperParams.pPreGain->value();
                if (pregain != c->sStateStage.fPV_shaper_pPreGain)
                {
                    c->sStateStage.fPV_shaper_pPreGain = pregain;
                    c->nUpdate |= UPD_SHAPER;
                }

                float postgain = c->sShaperParams.pPostGain->value();
                if (postgain != c->sStateStage.fPV_shaper_pPostGain)
                {
                    c->sStateStage.fPV_shaper_pPostGain = postgain;
                    c->nUpdate |= UPD_SHAPER;
                }

                float slope = c->sShaperParams.pSlope->value();
                if (slope != c->sStateStage.fPV_shaper_pSlope)
                {
                    c->sStateStage.fPV_shaper_pSlope = slope;
                    c->nUpdate |= UPD_SHAPER;
                }

                float shape = c->sShaperParams.pShape->value();
                if (shape != c->sStateStage.fPV_shaper_pShape)
                {
                    c->sStateStage.fPV_shaper_pShape = shape;
                    c->nUpdate |= UPD_SHAPER;
                }

                float highlevel = c->sShaperParams.pHighLevel->value();
                if (highlevel != c->sStateStage.fPV_shaper_pHighLevel)
                {
                    c->sStateStage.fPV_shaper_pHighLevel = highlevel;
                    c->nUpdate |= UPD_SHAPER;
                }

                float lowlevel = c->sShaperParams.pLowLevel->value();
                if (lowlevel != c->sStateStage.fPV_shaper_pLowLevel)
                {
                    c->sStateStage.fPV_shaper_pLowLevel = lowlevel;
                    c->nUpdate |= UPD_SHAPER;
                }

                float radius = c->sShaperParams.pRadius->value();
                if (radius != c->sStateStage.fPV_shaper_pRadius)
                {
                    c->sStateStage.fPV_shaper_pRadius = radius;
                    c->nUpdate |= UPD_SHAPER;
                }

                float levels = c->sShaperParams.pLevels->value();
                if (levels != c->sStateStage.fPV_shaper_pLevels)
                {
                    c->sStateStage.fPV_shaper_pLevels = levels;
                    c->nUpdate |= UPD_SHAPER;
                }

                float ccompanding = c->sShaperParams.pCCompanding->value();
                if (ccompanding != c->sStateStage.fPV_shaper_pCCompanding)
                {
                    c->sStateStage.fPV_shaper_pCCompanding = ccompanding;
                    c->nUpdate |= UPD_SHAPER;
                }

                float qcompanding = c->sShaperParams.pQCompanding->value();
                if (qcompanding != c->sStateStage.fPV_shaper_pQCompanding)
                {
                    c->sStateStage.fPV_shaper_pQCompanding = qcompanding;
                    c->nUpdate |= UPD_SHAPER;
                }

                float bias = c->sShaperParams.pBias->value();
                if (bias != c->sStateStage.fPV_shaper_pBias)
                {
                    c->sStateStage.fPV_shaper_pBias = bias;
                    c->nUpdate |= UPD_SHAPER;
                }

                float drive = c->sShaperParams.pDrive->value();
                if (drive != c->sStateStage.fPV_shaper_pDrive)
                {
                    c->sStateStage.fPV_shaper_pDrive = drive;
                    c->nUpdate |= UPD_SHAPER;
                }

                float blend = c->sShaperParams.pBlend->value();
                if (blend != c->sStateStage.fPV_shaper_pBlend)
                {
                    c->sStateStage.fPV_shaper_pBlend = blend;
                    c->nUpdate |= UPD_SHAPER;
                }

                size_t shaping = c->sShaperParams.pShapingFcn->value();
                if (shaping != c->sStateStage.nPV_shaper_pShapingFcn)
                {
                    c->sStateStage.nPV_shaper_pShapingFcn = shaping;
                    c->nUpdate |= UPD_SHAPER;
                }
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
                channel_t *c = &vChannels[i];

                commit_staged_state_change(c);

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
                    const size_t to_do      = lsp_min(samples - n, BUFFER_SIZE);
                    const size_t to_do_up   = c->sOversamplerParams.nOversampling * to_do;

                    c->sPreEQ.process(vBuffer, in, to_do);
                    dsp::mul_k2(vBuffer, c->sShaperParams.fPreGain, to_do);
                    c->sOversampler.upsample(vOSBuffer, vBuffer, to_do);
                    c->sShaper.process_overwrite(vOSBuffer, vOSBuffer, to_do_up);
                    c->sOversampler.downsample(vBuffer, vOSBuffer, to_do);
                    dsp::mul_k2(vBuffer, c->sShaperParams.fPostGain, to_do);
                    c->sPostEQ.process(vBuffer, vBuffer, to_do);

                    // Process the
                    //  - dry (unprocessed) signal stored in 'in'
                    //  - wet (processed) signal stored in 'vBuffer'
                    // Output the result to 'out' buffer
                    c->sBypass.process(out, in, vBuffer, to_do);

                    // Increment pointers
                    in                     +=  to_do;
                    out                    +=  to_do;
                    n                      +=  to_do;
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
            v->write("vOSBuffer", vOSBuffer);

            v->write("pBypass", pBypass);

            v->write("pData", pData);
        }

    } /* namespace plugins */
} /* namespace lsp */


