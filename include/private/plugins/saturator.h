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

#ifndef PRIVATE_PLUGINS_SATURATOR_H_
#define PRIVATE_PLUGINS_SATURATOR_H_

#include <lsp-plug.in/dsp-units/ctl/Bypass.h>
#include <lsp-plug.in/dsp-units/filters/Equalizer.h>
#include <lsp-plug.in/dsp-units/util/Oversampler.h>
#include <lsp-plug.in/dsp-units/shaping/Shaper.h>
#include <lsp-plug.in/plug-fw/plug.h>
#include <private/meta/saturator.h>

namespace lsp
{
    namespace plugins
    {
        /**
         * Base class for the latency compensation delay
         */
        class saturator: public plug::Module
        {
            protected:
//                enum ch_update_t
//                {
//                    UPD_PRE_STACK       = 1 << 0,
//                    UPD_OVERSAMPLER     = 1 << 1,
//                    UPD_STAGES          = 1 << 2,
//                    UPD_POST_STACK      = 1 << 3,
//                };

//                typedef struct ch_state_stage_t
//                {
//                    float  *vfPV_pre_eq_pGain;
//                    float  *vfPV_pre_eq_pSolo;
//                    float  *vfPV_pre_eq_pMute;
//                    float  *vfPV_pre_eq_pEnable;
//
//                    size_t  nPV_ovs_pMode;
//
//                    float   fPV_shaper_pPreGain;
//                    float   fPV_shaper_pPostGain;
//                    float   fPV_shaper_pSlope;
//                    float   fPV_shaper_pShape;
//                    float   fPV_shaper_pHighLevel;
//                    float   fPV_shaper_pLowLevel;
//                    float   fPV_shaper_pRadius;
//                    float   fPV_shaper_pLevels;
//                    float   fPV_shaper_pCCompanding;
//                    float   fPV_shaper_pQCompanding;
//                    float   fPV_shaper_pBias;
//                    float   fPV_shaper_pDrive;
//                    float   fPV_shaper_pBlend;
//                    size_t  nPV_shaper_pShapingFcn;
//
//                    float  *vfPV_post_eq_pGain;
//                    float  *vfPV_post_eq_pSolo;
//                    float  *vfPV_post_eq_pMute;
//                    float  *vfPV_post_eq_pEnable;
//                } ch_state_stage_t;

                // Same as Graph Equalizer
//                typedef struct eq_band_t
//                {
//                    dspu::filter_params_t sOldFP;                   // Old filter parameters
//                    dspu::filter_params_t sFP;                      // Filter parameters
//
////                    bool                    bHasSolo;               // Whether it has solo
////                    size_t                  nSoloBand;              // The number of the solo band
////
////                    plug::IPort            *pGain;                  // Gain port
////                    plug::IPort            *pSolo;                  // Solo port
////                    plug::IPort            *pMute;                  // Mute port
////                    plug::IPort            *pEnable;                // Enable port
//                } eq_band_t;

                typedef struct oversampler_t
                {
                    dspu::Oversampler       sOvs;                   // Oversampler

                    dspu::over_mode_t       enOverMode;             // Oversampler Mode
                    size_t                  nOversampling;          // Oversampling Factor
                    size_t                  nOverSampleRate;        // Oversampled Rate

                    plug::IPort            *pMode;                  // Oversampler mode

                    size_t                  nPV_pMode;
                    bool                    bRecompute;
                } oversampler_t;

                enum shaper_update_t
                {
                    UPD_SLOPE           = 1 << 0,
                    UPD_SHAPE           = 1 << 1,
                    UPD_HIGH_LEVEL      = 1 << 2,
                    UPD_LOW_LEVEL       = 1 << 3,
                    UPD_RADIUS          = 1 << 4,
                    UPD_LEVELS          = 1 << 5,
                    UPD_Q_COMPANDING    = 1 << 6,
                    UPD_C_COMPANDING    = 1 << 7,
                    UPD_BIAS            = 1 << 8,
                    UPD_DRIVE           = 1 << 9,
                    UPD_BLEND           = 1 << 10,
                    UPD_SHAPING_FCN     = 1 << 11,
                };

                typedef struct shaper_t
                {
                    dspu::Shaper            sShaper;                // The shaper

//                    float                   fPreGain;               // Pre Gain
//                    float                   fPostGain;              // Post Gain
                    float                   fSlope;                 // Slope (for sinusoidal saturator)
                    float                   fShape;                 // Shape (for many saturators)
                    float                   fHighLevel;             // High Level (for asymmetric saturators)
                    float                   fLowLevel;              // Low Level (for asymmetric saturators)
                    float                   fRadius;                // Radius (for quarter circle saturator)
                    float                   fLevels;                // Levels (for bitcrush)
                    float                   fCCompanding;           // Continuos companding (for continuous A-law and μ-law companders)
                    float                   fQCompanding;           // Quantized companding (for quantized A-law and μ-law companders)
                    float                   fBias;                  // Bias (for quantized μ-law)
                    float                   fDrive;                 // Drive (for TAP)
                    float                   fBlend;                 // Blend (for TAP)
                    dspu::sh_function_t     enShapingFcn;           // Shaping Function

//                    plug::IPort            *pPreGain;               // Pre Gain
//                    plug::IPort            *pPostGain;              // Post Gain
                    plug::IPort            *pSlope;                 // Slope (for sinusoidal saturator)
                    plug::IPort            *pShape;                 // Shape (for many saturators)
                    plug::IPort            *pHighLevel;             // High Level (for asymmetric saturators)
                    plug::IPort            *pLowLevel;              // Low Level (for asymmetric saturators)
                    plug::IPort            *pRadius;                // Radius (for quarter circle saturator)
                    plug::IPort            *pLevels;                // Levels (for bitcrush)
                    plug::IPort            *pCCompanding;           // Continuos companding (for continuous A-law and μ-law companders)
                    plug::IPort            *pQCompanding;           // Quantized companding (for quantized A-law and μ-law companders)
                    plug::IPort            *pBias;                  // Bias (for quantized μ-law)
                    plug::IPort            *pDrive;                 // Drive (for TAP)
                    plug::IPort            *pBlend;                 // Blend (for TAP)
                    plug::IPort            *pShapingFcn;            // Shaping Function

//                    float                   fPV_pPreGain;
//                    float                   fPV_pPostGain;
                    float                   fPV_pSlope;
                    float                   fPV_pShape;
                    float                   fPV_pHighLevel;
                    float                   fPV_pLowLevel;
                    float                   fPV_pRadius;
                    float                   fPV_pLevels;
                    float                   fPV_pCCompanding;
                    float                   fPV_pQCompanding;
                    float                   fPV_pBias;
                    float                   fPV_pDrive;
                    float                   fPV_pBlend;
                    size_t                  nPV_pShapingFcn;
//                    bool                    bRecompute;
                    size_t                  nUpdate;
                } shaper_t;

//                typedef struct tone_stack_bands_t
//                {
//                    float                   fBassBandLow;
//                    float                   fBassBandHigh;
//
//                    float                   fMidBandLow;
//                    float                   fMidBandHigh;
//
//                    float                   fTrebleBandLow;
//                    float                   fTrebleBandHigh;
//                } tone_stack_bands_t;

                enum filter_type_t
                {
                    LOSHELF     = dspu::FLT_MT_LRX_LOSHELF,
                    HISHEKF     = dspu::FLT_MT_LRX_HISHELF,
                    LADDERPASS  = dspu::FLT_MT_LRX_LADDERPASS,
                    NONE        = dspu::FLT_NONE,
                };

                typedef struct stack_band_t
                {
                    float                   fGain;
                    float                   fCenter;
                    float                   fLow;
                    float                   fHigh;
                    bool                    bEnabled;

                    dspu::filter_params_t   sOldFP;                   // Old filter parameters
                    dspu::filter_params_t   sFP;                      // Filter parameters

                    plug::IPort            *pGain;
                    plug::IPort            *pCenter;
                    plug::IPort            *pEnabled;

                    float                   fPV_pGain;
                    float                   fPV_pCenter;
                    float                   fPV_pEnabled;
                    bool                    bRecompute;
                } stack_band_t;

                typedef struct tone_stack_t
                {
                    dspu::Equalizer         sEQ;
//                    eq_band_t              *vBands;

                    stack_band_t            sSubBass;
                    stack_band_t            sBass;
                    stack_band_t            sMid;
                    stack_band_t            sHigh;
                    stack_band_t            sPresence;
                    stack_band_t            sAir;
                } tone_stack_t;

                typedef struct stage_t
                {
                    dspu::Bypass            sBypass;                // Bypass
                    float                   fGain;                  // Gain of the current stage
                    tone_stack_t            sToneStack;             // Tone Stack
                    shaper_t                sShaper;                // Shaper
                    bool                    bMeshSync;

                    plug::IPort            *pBypass;                // Bypass
                    plug::IPort            *pGain;                  // Gain port
                    plug::IPort            *pMesh;

                    float                   fPV_pGain;
                } stage_t;

                typedef struct channel_t
                {
//                    size_t                  nUpdate;
//                    ch_state_stage_t        sStateStage;

                    // DSP processing modules
                    dspu::Bypass            sBypass;                // Bypass

                    tone_stack_t            sPreStack;              // Pre Tone Stack
                    float                   fPreGain;

                    oversampler_t           sOversampler;           // Oversampler
                    stage_t                *vStages;                // Nonlinear Stages

                    tone_stack_t            sPostStack;             // Post Tone Stack
                    float                   fPostGain;

                    // Input ports
                    plug::IPort            *pIn;                    // Input port
                    plug::IPort            *pOut;                   // Output port

                    plug::IPort            *pPreGain;
//                    plug::IPort            *pPreBass;
//                    plug::IPort            *pPreMid;
//                    plug::IPort            *pPreTreble;

                    plug::IPort            *pPostGain;
//                    plug::IPort            *pPostBass;
//                    plug::IPort            *pPostMid;
//                    plug::IPort            *pPostTreble;

                    float                   fPV_pPreGain;
//                    float                   fPV_pPreBass;
//                    float                   fPV_pPreMid;
//                    float                   fPV_pPreTreble;
//                    bool                    bRecomputePre;

                    float                   fPV_pPostGain;
//                    float                   fPV_pPostBass;
//                    float                   fPV_pPostMid;
//                    float                   fPV_pPostTreble;
//                    float                   bRecomputePost;
                } channel_t;

            protected:
                size_t                      nSampleRate;            // Sample rate
                size_t                      nBands;                 // Number of bands for Pre and Post EQs of all channels.
                size_t                      nStages;                // Number of nonlinear stages in each channel
                size_t                      nChannels;              // Number of channels
                channel_t                  *vChannels;              // Delay channels
//                tone_stack_bands_t          sBands;
                float                      *vBuffer;                // Temporary buffer for audio processing
                float                      *vOSBuffer;              // Temporary buffer for oversampled audio processing
                float                      *vOSStageBuffer;         // Temporary buffer for Saturation stage processing. It contains oversampled data.
                float                      *VOSStageAccBuffer;      // Temporary buffer where we accumulate the results of the saturation stages.

                plug::IPort                *pBypass;                // Bypass
                plug::IPort                *pStageSel;
                plug::IPort                *pComment;               // Comment

//                const float                *vFreqs;                 // Pointer to the Frequency vector in metadata, a constant.

                uint8_t                    *pData;                  // Allocated data

            protected:
                void                            do_destroy();

            protected:
                static dspu::over_mode_t        get_oversampler_mode(size_t portValue);
                static dspu::sh_function_t      get_shaping_function(size_t portValue);

            protected:
                void                            init_stack_band_state_stage(stack_band_t *band, float center, bool enable);
                void                            init_state_stage(channel_t *c);
                void                            commit_staged_state_change(channel_t *c);
                void                            init_tone_stack(tone_stack_t *stack);
//                void                            setup_eq_filter(dspu::filter_params_t *fp, size_t band, float gain, bool mute, bool enable, bool has_solo, bool solo_band);
//                void                            calculate_tone_stack_bands(tone_stack_bands_t *bands, size_t samplerate);
                void                            configure_oversampler(oversampler_t *ovs);
                void                            configure_stage(stage_t *stage);
                void                            configure_stack_band(stack_band_t *band, filter_type_t type);
                void                            configure_tone_stack(tone_stack_t *stack);
                void                            process_stage(stage_t *stage, float *dst, const float *src, size_t count);
                void                            process_stages(channel_t *c, float *dst, const float *src, size_t count);

            public:
                virtual void                    update_sample_rate(long sr) override;
                virtual void                    update_settings() override;
                virtual void                    process(size_t samples) override;
                virtual void                    dump(dspu::IStateDumper *v) const override;

            public:
                virtual void                    init(plug::IWrapper *wrapper, plug::IPort **ports) override;
                virtual void                    destroy() override;

                explicit saturator(const meta::plugin_t *meta, size_t bands, size_t stages);
                saturator (const saturator &) = delete;
                saturator (saturator &&) = delete;
                virtual ~saturator() override;

                saturator & operator = (const saturator &) = delete;
                saturator & operator = (saturator &&) = delete;
        };

    } /* namespace plugins */
} /* namespace lsp */


#endif /* PRIVATE_PLUGINS_SATURATOR_H_ */

