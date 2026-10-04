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
#include <lsp-plug.in/stdlib/math.h>

#include <private/plugins/saturator.h>

namespace lsp
{
    namespace plugins
    {
        /* The size of temporary buffer for audio processing */
        static constexpr size_t BUFFER_SIZE                 = 0x200;
        static constexpr size_t STAGE_BUFFER_SIZE           = 0x200;

        // Tone stack limits
        static constexpr float DC_BLOCK_FREQUENCY           = 5.0f;
        static constexpr float MIN_BAND_FREQUENCY           = 20.0f;
        static constexpr float MAX_ANTI_ALIASING_FREQUENCY  = 20000.0f;
        static constexpr float NYQUIST_SCALER               = 5.0f / 6.0f;

        //---------------------------------------------------------------------
        // Plugin factory
        static const meta::plugin_t *plugins[] =
        {
            &meta::saturator_mono,
            &meta::saturator_stereo,
            &meta::saturator_lr,
        };

        typedef struct plugin_settings_t
        {
            const meta::plugin_t   *metadata;
            uint8_t                 bands;
            uint8_t                 stages;
        } plugin_settings_t;

        static const plugin_settings_t plugin_settings[] =
        {
            {&meta::saturator_mono,     meta::saturator::MAX_TONE_BANDS,  meta::saturator::MAX_STAGES   },
            {&meta::saturator_stereo,   meta::saturator::MAX_TONE_BANDS,  meta::saturator::MAX_STAGES   },
            {&meta::saturator_lr,       meta::saturator::MAX_TONE_BANDS,  meta::saturator::MAX_STAGES   },

            { NULL, 0 }
        };

        static plug::Module *plugin_factory(const meta::plugin_t *meta)
        {
            for (const plugin_settings_t *s = plugin_settings; s->metadata != NULL; ++s)
                if (s->metadata == meta)
                    return new saturator(s->metadata, s->bands, s->stages);

            return NULL;
        }

        static plug::Factory factory(plugin_factory, plugins, 2);

        //---------------------------------------------------------------------
        // Implementation
        saturator::saturator(const meta::plugin_t *meta, size_t bands, size_t stages):
            Module(meta)
        {
            // Compute the number of audio channels by the number of inputs
            nChannels = 0;
            for (const meta::port_t *p = meta->ports; p->id != NULL; ++p)
                if (meta::is_audio_in_port(p))
                    ++nChannels;

            // Initialize other parameters
            nSampleRate         = 0;
            nBands              = bands;
            nStages             = stages;
            vChannels           = NULL;
            vBuffer             = NULL;
            vOSBuffer           = NULL;
            vOSStageBuffer      = NULL;
            VOSStageAccBuffer   = NULL;

            pBypass             = NULL;
            pStageSel           = NULL;
            pComment            = NULL;

//            vFreqs          = vfreqs;

            pData               = NULL;
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

            // We have 1x audio buffer for processing:
            const size_t szof_buf           = BUFFER_SIZE * sizeof(float);
            // Then, we have 1x audio buffer for oversampled data.
            const size_t szof_osbuf         = szof_buf * meta::saturator::SAT_OVS_MAX;
            // Then, we have 2x audio buffer for temporary saturator stage processing:
            const size_t szof_stage_buf     = STAGE_BUFFER_SIZE * sizeof(float);
            const size_t szof_stage_bufs    = 2 * szof_stage_buf;
            // In total
            const size_t szos_bufs          = szof_buf + szof_osbuf + szof_stage_bufs;

            // We have nChannels `channel_t` objects:
            const size_t szof_channels      = align_size(sizeof(channel_t) * nChannels, OPTIMAL_ALIGN);
            /** In each channel we have multiple Tone stacks, each containing a vBands array of nBands stack bands.
             *  We have:
             *  1x Top level pre tone stack
             *  1x Top level post tone stack
             *  1x tone stack in each saturator stage, nStages stages in total.
             *  Each channel has saturation stages.
             *  Therefore:
             */
            const size_t szof_eq_bands      = align_size(sizeof(eq_band_t) * nBands, OPTIMAL_ALIGN);
            const size_t szof_ch_eqs        = (2 + nStages) * szof_eq_bands;
            const size_t szof_eqs           = szof_ch_eqs * nChannels;
            // We have nStages saturation stages per channel
            const size_t szof_stage_array   = align_size(sizeof(stage_t) * nStages, OPTIMAL_ALIGN);
            const size_t szof_stages        = szof_stage_array * nChannels;
            // Each stage has a tone stack, each channel has an array of stages
            const size_t szof_tone_stack    = align_size(sizeof(tone_stack_t) * nStages, OPTIMAL_ALIGN);
            const size_t szof_tone_stacks   = szof_tone_stack * nChannels;


//            /** Each `ch_state_stage_t` object has:
//             * 1x vfPV_pre_eq_pGain vector of nBands float
//             * 1x vfPV_pre_eq_pSolo vector of nBands float
//             * 1x vfPV_pre_eq_pMute vector of nBands float
//             * 1x vfPV_pre_eq_pEnable vector of nBands float
//             * 1x vfPV_post_eq_pGain vector of nBands float
//             * 1x vfPV_post_eq_pSolo vector of nBands float
//             * 1x vfPV_post_eq_pMute vector of nBands float
//             * 1x vfPV_post_eq_pEnable vector of nBands float
//             *
//             * So, 8 arrays of nBands floats per `ch_state_stage_t` object.
//             * We have a `ch_state_stage_t` object per channel. So:
//             */
//            const size_t szof_stage_array   = nBands * sizeof(float);

            // In total:
            const size_t alloc =
                    szos_bufs +         // All audio buffers
                    szof_channels +     // Top level channel size
                    szof_eqs +          // Size of all EQ bands
                    szof_stages +       // Size of all nonlinear stages
                    szof_tone_stacks;   // Size of all tone stacks

            // Allocate memory-aligned data
            uint8_t *ptr = alloc_aligned<uint8_t>(pData, alloc, OPTIMAL_ALIGN);
            if (ptr == NULL)
                return;
            lsp_guard_assert(uint8_t * const save = ptr);

            // Assign all resources
            vChannels = advance_ptr_bytes<channel_t>(ptr, szof_channels);

            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t *c = &vChannels[i];

                c->sBypass.construct();

                c->sPreStack.vBands = advance_ptr_bytes<eq_band_t>(ptr, szof_eq_bands);
                c->sPreStack.sEQ.construct();
                c->sPreStack.sEQ.init(nBands, meta::saturator::FFT_RANK);

                c->sOversampler.sOvs.construct();
                c->sOversampler.sOvs.init();

                c->vStages = advance_ptr_bytes<stage_t>(ptr, szof_stage_array);
                for (size_t s=0; s<nStages; ++s)
                {
                    c->vStages[s].sBypass.construct();

                    c->vStages[s].sShaper.sShaper.construct();

                    c->vStages[s].sToneStack.vBands = advance_ptr_bytes<eq_band_t>(ptr, szof_eq_bands);
                    c->vStages[s].sToneStack.sEQ.construct();
                    c->vStages[s].sToneStack.sEQ.init(nBands, meta::saturator::FFT_RANK);
                }

                c->sPostStack.vBands = advance_ptr_bytes<eq_band_t>(ptr, szof_eq_bands);
                c->sPostStack.sEQ.construct();
                c->sPostStack.sEQ.init(nBands, meta::saturator::FFT_RANK);

//                c->sStateStage.vfPV_pre_eq_pGain        = advance_ptr_bytes<float>(ptr, szof_stage_array);
//                c->sStateStage.vfPV_pre_eq_pSolo        = advance_ptr_bytes<float>(ptr, szof_stage_array);
//                c->sStateStage.vfPV_pre_eq_pMute        = advance_ptr_bytes<float>(ptr, szof_stage_array);
//                c->sStateStage.vfPV_pre_eq_pEnable      = advance_ptr_bytes<float>(ptr, szof_stage_array);
//
//                c->sStateStage.vfPV_post_eq_pGain       = advance_ptr_bytes<float>(ptr, szof_stage_array);
//                c->sStateStage.vfPV_post_eq_pSolo       = advance_ptr_bytes<float>(ptr, szof_stage_array);
//                c->sStateStage.vfPV_post_eq_pMute       = advance_ptr_bytes<float>(ptr, szof_stage_array);
//                c->sStateStage.vfPV_post_eq_pEnable     = advance_ptr_bytes<float>(ptr, szof_stage_array);
//
//                // Only now we are ready to initialize the stage. Allocation must happen first.
//                init_state_stage(c);

                c->pIn                  = NULL;
                c->pOut                 = NULL;
            }

            vBuffer                 = advance_ptr_bytes<float>(ptr, szof_buf);
            vOSBuffer               = advance_ptr_bytes<float>(ptr, szof_osbuf);
            vOSStageBuffer          = advance_ptr_bytes<float>(ptr, szof_stage_buf);
            VOSStageAccBuffer       = advance_ptr_bytes<float>(ptr, szof_stage_buf);

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
            BIND_PORT(pStageSel);

            // Bind saturator control ports
            lsp_trace("Binding saturator control ports");
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c = &vChannels[i];

                BIND_PORT(c->sOversampler.pMode);

                BIND_PORT(c->pPreGain);
                BIND_PORT(c->pPreBass);
                BIND_PORT(c->pPreMid);
                BIND_PORT(c->pPreTreble);

                BIND_PORT(c->pPostGain);
                BIND_PORT(c->pPostBass);
                BIND_PORT(c->pPostMid);
                BIND_PORT(c->pPostTreble);

                for (size_t s=0; s<nStages; ++s)
                {
                    BIND_PORT(c->vStages[s].pGain);

                    BIND_PORT(c->vStages[s].sToneStack.pBass);
                    BIND_PORT(c->vStages[s].sToneStack.pMid);
                    BIND_PORT(c->vStages[s].sToneStack.pTreble);

                    BIND_PORT(c->vStages[s].sShaper.pSlope);
                    BIND_PORT(c->vStages[s].sShaper.pShape);
                    BIND_PORT(c->vStages[s].sShaper.pHighLevel);
                    BIND_PORT(c->vStages[s].sShaper.pLowLevel);
                    BIND_PORT(c->vStages[s].sShaper.pRadius);
                    BIND_PORT(c->vStages[s].sShaper.pLevels);
                    BIND_PORT(c->vStages[s].sShaper.pCCompanding);
                    BIND_PORT(c->vStages[s].sShaper.pQCompanding);
                    BIND_PORT(c->vStages[s].sShaper.pBias);
                    BIND_PORT(c->vStages[s].sShaper.pDrive);
                    BIND_PORT(c->vStages[s].sShaper.pBlend);
                    BIND_PORT(c->vStages[s].sShaper.pShapingFcn);

                    BIND_PORT(c->vStages[s].pMesh);
                }
            }

            BIND_PORT(pComment);
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
                    channel_t *c = &vChannels[i];
                    c->sBypass.destroy();
                    c->sPreStack.sEQ.destroy();
                    c->sOversampler.sOvs.destroy();

                    for (size_t s = 0; s>nStages; ++s)
                    {
                        c->vStages[s].sBypass.destroy();

                        c->vStages[s].sToneStack.sEQ.destroy();
                        c->vStages[s].sToneStack.vBands = NULL;

                        c->vStages[s].sShaper.sShaper.destroy();
                    }
                    c->vStages = NULL;

                    c->sPostStack.sEQ.destroy();
                }
                vChannels   = NULL;
            }

            vBuffer             = NULL;
            vOSBuffer           = NULL;
            vOSStageBuffer      = NULL;
            VOSStageAccBuffer   = NULL;

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

            calculate_tone_stack_bands(&sBands, nSampleRate);

            // Update sample rate for the bypass processors
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c = &vChannels[i];
                c->sBypass.init(nSampleRate);

                c->sPreStack.sEQ.set_sample_rate(nSampleRate);
                configure_tone_stack(&c->sPreStack, &sBands);

                c->sOversampler.nOverSampleRate = c->sOversampler.nOversampling * nSampleRate;

                for (size_t s = 0; s>nStages; ++s)
                {
                    c->vStages[s].sBypass.init(c->sOversampler.nOverSampleRate);

                    c->vStages[s].sToneStack.sEQ.set_sample_rate(c->sOversampler.nOverSampleRate);
                    configure_tone_stack(&c->vStages[s].sToneStack, &sBands);

                    c->vStages[s].sShaper.sShaper.set_sample_rate(c->sOversampler.nOverSampleRate);
                }

                c->sPostStack.sEQ.set_sample_rate(nSampleRate);
                configure_tone_stack(&c->sPostStack, &sBands);
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
            c->fPV_pPreGain     = meta::saturator::PRE_GAIN_DFL;
            c->fPV_pPreBass     = meta::saturator::BAND_GAIN_DFL;
            c->fPV_pPreMid      = meta::saturator::BAND_GAIN_DFL;
            c->fPV_pPreTreble   = meta::saturator::BAND_GAIN_DFL;
            c->bRecomputePre    = true;

            c->fPV_pPostGain    = meta::saturator::POST_GAIN_DFL;
            c->fPV_pPostBass    = meta::saturator::BAND_GAIN_DFL;
            c->fPV_pPostMid     = meta::saturator::BAND_GAIN_DFL;
            c->fPV_pPostTreble  = meta::saturator::BAND_GAIN_DFL;
            c->bRecomputePost   = true;

            c->sOversampler.nPV_pMode   = meta::saturator::SAT_OVS_DFL;
            c->sOversampler.bRecompute  = true;

            for (size_t s=0; s<nStages; ++s)
            {
                c->vStages[s].fPV_pGain                 = meta::saturator::STAGE_GAIN_DFL;

                c->vStages[s].sToneStack.fPV_pBass      = meta::saturator::BAND_GAIN_DFL;
                c->vStages[s].sToneStack.fPV_pMid       = meta::saturator::BAND_GAIN_DFL;
                c->vStages[s].sToneStack.fPV_pTreble    = meta::saturator::BAND_GAIN_DFL;
                c->vStages[s].sToneStack.bRecompute     = true;

//                c->vStages[s].sShaper.fPV_pPreGain      = meta::saturator::PRE_GAIN_DFL;
//                c->vStages[s].sShaper.fPV_pPostGain     = meta::saturator::POST_GAIN_DFL;
                c->vStages[s].sShaper.fPV_pSlope        = meta::saturator::SLOPE_DFL;
                c->vStages[s].sShaper.fPV_pShape        = meta::saturator::SHAPE_DFL;
                c->vStages[s].sShaper.fPV_pHighLevel    = meta::saturator::HIGH_LEVEL_DFL;
                c->vStages[s].sShaper.fPV_pLowLevel     = meta::saturator::LOW_LEVEL_DFL;
                c->vStages[s].sShaper.fPV_pRadius       = meta::saturator::RADIUS_DFL;
                c->vStages[s].sShaper.fPV_pLevels       = meta::saturator::LEVELS_DFL;
                c->vStages[s].sShaper.fPV_pCCompanding  = meta::saturator::C_COMPANDING_DFL;
                c->vStages[s].sShaper.fPV_pQCompanding  = meta::saturator::Q_COMPANDING_DFL;
                c->vStages[s].sShaper.fPV_pBias         = meta::saturator::BIAS_DFL;
                c->vStages[s].sShaper.fPV_pDrive        = meta::saturator::DRIVE_DFL;
                c->vStages[s].sShaper.fPV_pBlend        = meta::saturator::BLEND_DFL;
                c->vStages[s].sShaper.nPV_pShapingFcn   = meta::saturator::SAT_SH_FCN_DEFAULT;
                c->vStages[s].sShaper.bRecompute        = true;
            }
        }

        void saturator::commit_staged_state_change(channel_t *c)
        {
            c->fPreGain = c->fPV_pPreGain;

            if (c->bRecomputePre)
            {
                configure_tone_stack(&c->sPreStack, &sBands);
                c->bRecomputePre = false;
            }

            c->fPostGain = c->fPV_pPostGain;

            if (c->bRecomputePost)
            {
                configure_tone_stack(&c->sPostStack, &sBands);
                c->bRecomputePost = false;
            }

            if (c->sOversampler.bRecompute)
            {
                c->sOversampler.enOverMode = get_oversampler_mode(c->sOversampler.nPV_pMode);

                c->sOversampler.sOvs.set_mode(c->sOversampler.enOverMode);
                if (c->sOversampler.sOvs.modified())
                    c->sOversampler.sOvs.update_settings();

                c->sOversampler.nOversampling   = c->sOversampler.sOvs.get_oversampling();
                c->sOversampler.nOverSampleRate = nSampleRate * c->sOversampler.nOversampling;

                c->sOversampler.bRecompute = false;
            }

            for (size_t s=0; s<nStages; ++s)
            {
                stage_t* stage = &c->vStages[s];

                stage->fGain = stage->fPV_pGain;

                if (stage->sToneStack.bRecompute)
                {
                    configure_tone_stack(&stage->sToneStack, &sBands);
                    c->vStages[s].sToneStack.bRecompute = false;
                }

                // TODO:  Do these selectively with a boolean update flag?
                if  (stage->sShaper.bRecompute)
                {
                    stage->sShaper.fSlope = stage->sShaper.fPV_pSlope;
                    stage->sShaper.sShaper.set_slope(stage->sShaper.fSlope);

                    stage->sShaper.fShape = stage->sShaper.fPV_pShape;
                    stage->sShaper.sShaper.set_shape(stage->sShaper.fShape);

                    stage->sShaper.fHighLevel = stage->sShaper.fPV_pHighLevel;
                    stage->sShaper.sShaper.set_high_level(stage->sShaper.fHighLevel);

                    stage->sShaper.fLowLevel = stage->sShaper.fPV_pLowLevel;
                    stage->sShaper.sShaper.set_low_level(stage->sShaper.fLowLevel);

                    stage->sShaper.fRadius = stage->sShaper.fPV_pRadius;
                    stage->sShaper.sShaper.set_radius(stage->sShaper.fRadius);

                    stage->sShaper.fLevels = stage->sShaper.fPV_pLevels;
                    stage->sShaper.sShaper.set_levels(stage->sShaper.fLevels);

                    stage->sShaper.fCCompanding = stage->sShaper.fPV_pCCompanding;
                    stage->sShaper.sShaper.set_continuous_companding(stage->sShaper.fCCompanding);

                    stage->sShaper.fQCompanding = stage->sShaper.fPV_pQCompanding;
                    stage->sShaper.sShaper.set_quantized_companding(stage->sShaper.fQCompanding);

                    stage->sShaper.fBias = stage->sShaper.fPV_pBias;
                    stage->sShaper.sShaper.set_bias(stage->sShaper.fBias);

                    stage->sShaper.fDrive = stage->sShaper.fPV_pDrive;
                    stage->sShaper.sShaper.set_drive(stage->sShaper.fDrive);

                    stage->sShaper.fBlend = stage->sShaper.fPV_pBlend;
                    stage->sShaper.sShaper.set_blend(stage->sShaper.fBlend);

                    stage->sShaper.enShapingFcn = get_shaping_function(stage->sShaper.nPV_pShapingFcn);
                    stage->sShaper.sShaper.set_function(stage->sShaper.enShapingFcn);

                    stage->sShaper.bRecompute = false;
                }
            }
        }

        void saturator::calculate_tone_stack_bands(tone_stack_bands_t *bands, size_t samplerate)
        {
            float nyquist           = samplerate / 2;
            float lowest            = MIN_BAND_FREQUENCY;
            float highest           = lsp::lsp_min(MAX_ANTI_ALIASING_FREQUENCY, NYQUIST_SCALER * nyquist);

            float exponent          = 1.0f / nBands;
            float band_finder       = powf(highest / lowest, exponent);

            bands->fBassBandLow     = lsp::lsp_min(DC_BLOCK_FREQUENCY, lowest);
            bands->fBassBandHigh    = bands->fBassBandLow * band_finder;

            bands->fMidBandLow      = bands->fBassBandHigh;
            bands->fMidBandHigh     = bands->fMidBandLow * band_finder;

            bands->fTrebleBandLow   = bands->fMidBandHigh;
            bands->fTrebleBandHigh  = highest;
        }

        void saturator::configure_tone_stack(tone_stack_t *stack, const tone_stack_bands_t *bands)
        {
            dspu::filter_params_t *fp;

            eq_band_t *bass                 = &stack->vBands[0];
            fp                              = &bass->sFP;
            fp->nSlope                      = 1;
            fp->fQuality                    = 0.0f;
            fp->fGain                       = stack->fBass;
            fp->nType                       = dspu::FLT_DR_APO_BANDPASS;
            fp->fFreq                       = bands->fBassBandLow;
            fp->fFreq2                      = bands->fTrebleBandHigh;
            stack->sEQ.set_params(0, fp);

            eq_band_t *mid                  = &stack->vBands[1];
            fp                              = &mid->sFP;
            fp->nSlope                      = 1;
            fp->fQuality                    = 0.0f;
            fp->fGain                       = stack->fMid;
            fp->nType                       = dspu::FLT_DR_APO_BANDPASS;
            fp->fFreq                       = bands->fMidBandLow;
            fp->fFreq2                      = bands->fMidBandHigh;
            stack->sEQ.set_params(1, fp);

            eq_band_t *treble               = &stack->vBands[1];
            fp                              = &treble->sFP;
            fp->nSlope                      = 1;
            fp->fQuality                    = 0.0f;
            fp->fGain                       = stack->fTreble;
            fp->nType                       = dspu::FLT_DR_APO_BANDPASS;
            fp->fFreq                       = bands->fTrebleBandLow;
            fp->fFreq2                      = bands->fTrebleBandHigh;
            stack->sEQ.set_params(2, fp);
        }

//        void saturator::setup_eq_filter(dspu::filter_params_t *fp, size_t band, float gain, bool mute, bool enable, bool has_solo, bool solo_band)
//        {
//            // TODO: As we add more settings for the filter bands we will need to to change this,
//
//            fp->nSlope      = 1;
//            fp->fQuality    = 0.0f;
//            fp->fGain       = gain;
//
//            if (band == 0)
//            {
//                fp->nType   = dspu::FLT_DR_APO_LOPASS;
//                fp->fFreq   = sqrtf(vFreqs[0] * vFreqs[1]);
//                fp->fFreq2  = fp->fFreq;
//            }
//            else if (band == (nBands - 1))
//            {
//                fp->nType   = dspu::FLT_DR_APO_HIPASS;
//                fp->fFreq   = sqrtf(vFreqs[band-1] * vFreqs[band]);
//                fp->fFreq2  = fp->fFreq;
//            }
//            else
//            {
//                fp->nType   = dspu::FLT_DR_APO_BANDPASS;
//                fp->fFreq   = sqrtf(vFreqs[band-1] * vFreqs[band]);
//                fp->fFreq2  = sqrtf(vFreqs[band] * vFreqs[band+1]);
//            }
//
//            if (has_solo && (band != solo_band))
//            {
//                fp->nType   = dspu::FLT_NONE;
//                fp->fGain   = meta::saturator::BAND_GAIN_MIN;
//            }
//
//            if (mute >= 0.5f)
//            {
//                fp->nType   = dspu::FLT_NONE;
//                fp->fGain   = meta::saturator::BAND_GAIN_MIN;
//            }
//
//            if (enable < 0.5f)
//            {
//                fp->nType   = dspu::FLT_NONE;
//                fp->fGain   = meta::saturator::BAND_GAIN_MIN;
//            }
//        }

        void saturator::process_stage(stage_t *stage, float *dst, const float *src, size_t count)
        {
            for (size_t n=0; n<count;)
            {
                const size_t to_do = lsp_min(count - n, STAGE_BUFFER_SIZE);

                dsp::mul_k3(vOSStageBuffer, src, stage->fGain, to_do);
                stage->sToneStack.sEQ.process(vOSStageBuffer, vOSStageBuffer, to_do);
                stage->sShaper.sShaper.process_overwrite(vOSStageBuffer, vOSStageBuffer, to_do);
                stage->sBypass.process(dst, src, vOSStageBuffer, to_do);

                dst     += to_do;
                src     += to_do;
                n       += to_do;
            }
        }

        void saturator::process_stages(channel_t *c, float *dst, const float *src, size_t count)
        {
            for (size_t n=0; n<count;)
            {
                const size_t to_do = lsp_min(count - n, STAGE_BUFFER_SIZE);

                dsp::copy(VOSStageAccBuffer, src, to_do);

                for (size_t s=0; s<nStages; ++s)
                {
                    process_stage(&c->vStages[s], VOSStageAccBuffer, VOSStageAccBuffer, to_do);
                    dsp::copy(dst, VOSStageAccBuffer, to_do);

                    dst     += to_do;
                    src     += to_do;
                    n       += to_do;
                }

            }
        }

        void saturator::update_settings()
        {
            bool bypass = pBypass->value() >= 0.5f;

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c = &vChannels[i];

                c->sBypass.set_bypass(bypass);

                // Pre

                float pre_gain = c->pPreGain->value();
                if (pre_gain != c->fPV_pPreGain)
                {
                    c->fPV_pPreGain     = pre_gain;
                }

                float pre_bass = c->pPreBass->value();
                if (pre_bass != c->fPV_pPreBass)
                {
                    c->fPV_pPreBass     = pre_bass;
                    c->bRecomputePre    = true;
                }

                float pre_mid = c->pPreMid->value();
                if (pre_mid != c->fPV_pPreMid)
                {
                    c->fPV_pPreMid      = pre_mid;
                    c->bRecomputePre    = true;
                }

                float pre_treble = c->pPreTreble->value();
                if (pre_treble != c->fPV_pPreTreble)
                {
                    c->fPV_pPreTreble   = pre_treble;
                    c->bRecomputePre    = true;
                }

                // Post

                float post_gain = c->pPostGain->value();
                if (post_gain != c->fPV_pPostGain)
                {
                    c->fPV_pPostGain    = post_gain;
                }

                float post_bass = c->pPostBass->value();
                if (post_bass != c->fPV_pPostBass)
                {
                    c->fPV_pPostBass    = post_bass;
                    c->bRecomputePost   = true;
                }

                float post_mid = c->pPostMid->value();
                if (post_mid != c->fPV_pPostMid)
                {
                    c->fPV_pPostMid     = post_mid;
                    c->bRecomputePost   = true;
                }

                float post_treble = c->pPostTreble->value();
                if (post_treble != c->fPV_pPostTreble)
                {
                    c->fPV_pPostTreble  = post_treble;
                    c->bRecomputePre    = true;
                }

                // Oversampler

                size_t overmode = c->sOversampler.pMode->value();
                if (overmode != c->sOversampler.nPV_pMode)
                {
                    c->sOversampler.nPV_pMode   = overmode;
                    c->sOversampler.bRecompute  = true;
                }

                // Stages

                for (size_t s=0; s<nStages; ++s)
                {
                    stage_t* stage = &c->vStages[s];

                    stage->sBypass.set_bypass(stage->pBypass->value() >= 0.5f);

                    float gain = stage->pGain->value();
                    if (gain != stage->fPV_pGain)
                    {
                        stage->fPV_pGain = gain;
                    }

                    float bass = stage->sToneStack.pBass->value();
                    if (bass != stage->sToneStack.fPV_pBass)
                    {
                        stage->sToneStack.fPV_pBass     = bass;
                        stage->sToneStack.bRecompute    = true;
                    }

                    float mid = stage->sToneStack.pMid->value();
                    if (mid != stage->sToneStack.fPV_pMid)
                    {
                        stage->sToneStack.fPV_pMid      = mid;
                        stage->sToneStack.bRecompute    = true;
                    }

                    float treble = stage->sToneStack.pTreble->value();
                    if (treble != stage->sToneStack.fPV_pTreble)
                    {
                        stage->sToneStack.fPV_pTreble   = treble;
                        stage->sToneStack.bRecompute    = true;
                    }

                    float slope = stage->sShaper.pSlope->value();
                    if (slope != stage->sShaper.fPV_pSlope)
                    {
                        stage->sShaper.fPV_pSlope       = slope;
                        stage->sShaper.bRecompute       = true;
                    }

                    float shape = stage->sShaper.pShape->value();
                    if (shape != stage->sShaper.fPV_pShape)
                    {
                        stage->sShaper.fPV_pShape       = shape;
                        stage->sShaper.bRecompute       = true;
                    }

                    float high_level = stage->sShaper.pHighLevel->value();
                    if (high_level != stage->sShaper.fPV_pHighLevel)
                    {
                        stage->sShaper.fPV_pHighLevel   = high_level;
                        stage->sShaper.bRecompute       = true;
                    }

                    float low_level = stage->sShaper.pLowLevel->value();
                    if (low_level != stage->sShaper.fPV_pLowLevel)
                    {
                        stage->sShaper.fPV_pLowLevel    = low_level;
                        stage->sShaper.bRecompute       = true;
                    }

                    float radius = stage->sShaper.pRadius->value();
                    if (radius != stage->sShaper.fPV_pRadius)
                    {
                        stage->sShaper.fPV_pRadius      = radius;
                        stage->sShaper.bRecompute       = true;
                    }

                    float levels = stage->sShaper.pLevels->value();
                    if (levels != stage->sShaper.fPV_pLevels)
                    {
                        stage->sShaper.fPV_pLevels      = levels;
                        stage->sShaper.bRecompute       = true;
                    }

                    float c_comp = stage->sShaper.pCCompanding->value();
                    if (c_comp != stage->sShaper.fPV_pCCompanding)
                    {
                        stage->sShaper.fPV_pCCompanding = c_comp;
                        stage->sShaper.bRecompute       = true;
                    }

                    float q_comp = stage->sShaper.pQCompanding->value();
                    if (q_comp != stage->sShaper.fPV_pQCompanding)
                    {
                        stage->sShaper.fPV_pQCompanding = q_comp;
                        stage->sShaper.bRecompute       = true;
                    }

                    float bias = stage->sShaper.pBias->value();
                    if (bias != stage->sShaper.fPV_pBias)
                    {
                        stage->sShaper.fPV_pBias        = bias;
                        stage->sShaper.bRecompute       = true;
                    }

                    float drive = stage->sShaper.pDrive->value();
                    if (drive != stage->sShaper.fPV_pDrive)
                    {
                        stage->sShaper.fPV_pDrive       = drive;
                        stage->sShaper.bRecompute       = true;
                    }

                    float blend = stage->sShaper.pBlend->value();
                    if (blend != stage->sShaper.fPV_pBlend)
                    {
                        stage->sShaper.fPV_pBlend       = blend;
                        stage->sShaper.bRecompute       = true;
                    }

                    size_t shaping = stage->sShaper.pShapingFcn->value();
                    if (shaping != stage->sShaper.nPV_pShapingFcn)
                    {
                        stage->sShaper.nPV_pShapingFcn  = shaping;
                        stage->sShaper.bRecompute       = true;
                    }
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
                    const size_t to_do_up   = c->sOversampler.nOversampling * to_do;

                    c->sPreStack.sEQ.process(vBuffer, in, to_do);
                    dsp::mul_k2(vBuffer, c->fPreGain, to_do);

                    c->sOversampler.sOvs.upsample(vOSBuffer, vBuffer, to_do);
                    process_stages(c, vOSBuffer, vOSBuffer, to_do_up);
                    c->sOversampler.sOvs.downsample(vBuffer, vOSBuffer, to_do);

                    dsp::mul_k2(vBuffer, c->fPostGain, to_do);
                    c->sPostStack.sEQ.process(vBuffer, vBuffer, to_do);

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
            // TODO: Implement
//            plug::Module::dump(v);
//
//            // It is very useful to dump plugin state for debug purposes
//            v->write("nChannels", nChannels);
//            v->begin_array("vChannels", vChannels, nChannels);
//            for (size_t i=0; i<nChannels; ++i)
//            {
//                channel_t *c            = &vChannels[i];
//
//                v->begin_object(c, sizeof(channel_t));
//                {
//                    v->write_object("sBypass", &c->sBypass);
//                    v->write_object("sPreEQ", &c->sPreEQ);
//                    v->write_object("sOversampler", &c->sOversampler);
//                    v->write_object("sShaper", &c->sShaper);
//                    v->write_object("sPostEQ", &c->sPostEQ);
//
//                    // TODO: Dump the various parameters structures
//
//                    v->write("pIn", c->pIn);
//                    v->write("pOut", c->pOut);
//                }
//                v->end_object();
//            }
//            v->end_array();
//
//            v->write("vBuffer", vBuffer);
//            v->write("vOSBuffer", vOSBuffer);
//
//            v->write("pBypass", pBypass);
//
//            v->write("pData", pData);
        }

    } /* namespace plugins */
} /* namespace lsp */


