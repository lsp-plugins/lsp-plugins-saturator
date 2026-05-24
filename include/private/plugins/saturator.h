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
                // Same as Graph Equalizer
                typedef struct eq_band_t
                {
                    plug::IPort            *pGain;                  // Gain port
                    plug::IPort            *pSolo;                  // Solo port
                    plug::IPort            *pMute;                  // Mute port
                    plug::IPort            *pEnable;                // Enable port
                } eq_band_t;

                typedef struct oversampler_t
                {
                    dspu::over_mode_t       enOverMode;             // Oversampler Mode
                    size_t                  nOversampling;          // Oversampling Factor
                    size_t                  nOverSampleRate;        // Oversampled Rate

                    plug::IPort            *pMode;                  // Oversampler mode
                } oversampler_t;

                typedef struct shaper_t
                {
                    float                   fPreGain;               // Pre Gain
                    float                   fPostGain;              // Post Gain
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

                    plug::IPort            *pPreGain;               // Pre Gain
                    plug::IPort            *pPostGain;              // Post Gain
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
                } shaper_t;

                typedef struct channel_t
                {
                    // DSP processing modules
                    dspu::Bypass            sBypass;                // Bypass
                    dspu::Equalizer         sPreEQ;                 // Pre EQ
                    dspu::Oversampler       sOversampler;           // Oversampler
                    dspu::Shaper            sShaper;                // Nonlinearity
                    dspu::Equalizer         sPostEQ;                // Post EQ

                    // Parameters
                    eq_band_t              *vPreEQBands;            // Pre EQ Bands
                    oversampler_t           sOversamplerParams;     // Oversampler Parameters
                    shaper_t                sShaperParams;          // Shaper Parameters
                    eq_band_t              *vPostEQBands;           // Post EQ Bands

                    // Input ports
                    plug::IPort            *pIn;                    // Input port
                    plug::IPort            *pOut;                   // Output port

                    // Output ports
                    // TODO: Add ports
                } channel_t;

            protected:
                size_t                      nSampleRate;            // Sample rate
                size_t                      nBands;                 // Number of bands for Pre and Post EQs of all channels.
                size_t                      nChannels;              // Number of channels
                channel_t                  *vChannels;              // Delay channels
                float                      *vBuffer;                // Temporary buffer for audio processing

                plug::IPort                *pBypass;                // Bypass
                plug::IPort                *pComment;               // Comment

                uint8_t                    *pData;                  // Allocated data

            protected:
                void                do_destroy();

            public:
                explicit saturator(const meta::plugin_t *meta, size_t bands);
                saturator (const saturator &) = delete;
                saturator (saturator &&) = delete;
                virtual ~saturator() override;

                saturator & operator = (const saturator &) = delete;
                saturator & operator = (saturator &&) = delete;

                virtual void        init(plug::IWrapper *wrapper, plug::IPort **ports) override;
                virtual void        destroy() override;

            public:
                virtual void        update_sample_rate(long sr) override;
                virtual void        update_settings() override;
                virtual void        process(size_t samples) override;
                virtual void        dump(dspu::IStateDumper *v) const override;
        };

    } /* namespace plugins */
} /* namespace lsp */


#endif /* PRIVATE_PLUGINS_SATURATOR_H_ */

