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

#ifndef PRIVATE_META_SATURATOR_H_
#define PRIVATE_META_SATURATOR_H_

#include <lsp-plug.in/plug-fw/meta/types.h>
#include <lsp-plug.in/plug-fw/const.h>

namespace lsp
{
    //-------------------------------------------------------------------------
    // Plugin metadata
    namespace meta
    {
        typedef struct saturator
        {
            static constexpr float          BAND_GAIN_MIN       = GAIN_AMP_M_36_DB;
            static constexpr float          BAND_GAIN_MAX       = GAIN_AMP_P_36_DB;
            static constexpr float          BAND_GAIN_DFL       = GAIN_AMP_0_DB;
            static constexpr float          BAND_GAIN_STEP      = 0.025f;

            static constexpr size_t         FFT_RANK            = 13;

            enum oversampler_mode_selector_t
            {
                SAT_OVS_NONE,
                SAT_OVS_2X,
                SAT_OVS_3X,
                SAT_OVS_4X,
                SAT_OVS_6X,
                SAT_OVS_8X,

                SAT_OVS_DFL = SAT_OVS_8X
            };

            static constexpr float          PRE_GAIN_MIN       = GAIN_AMP_M_36_DB;
            static constexpr float          PRE_GAIN_MAX       = GAIN_AMP_P_36_DB;
            static constexpr float          PRE_GAIN_DFL       = GAIN_AMP_0_DB;
            static constexpr float          PRE_GAIN_STEP      = 0.025f;

            static constexpr float          POST_GAIN_MIN       = GAIN_AMP_M_36_DB;
            static constexpr float          POST_GAIN_MAX       = GAIN_AMP_P_36_DB;
            static constexpr float          POST_GAIN_DFL       = GAIN_AMP_0_DB;
            static constexpr float          POST_GAIN_STEP      = 0.025f;

            static constexpr float          SLOPE_MIN           = 0.0f;
            static constexpr float          SLOPE_MAX           = 1.0f;
            static constexpr float          SLOPE_DFL           = 0.5f;
            static constexpr float          SLOPE_STEP          = 0.001f;

            static constexpr float          SHAPE_MIN           = 0.0f;
            static constexpr float          SHAPE_MAX           = 1.0f;
            static constexpr float          SHAPE_DFL           = 0.5f;
            static constexpr float          SHAPE_STEP          = 0.001f;

            static constexpr float          HIGH_LEVEL_MIN      = 0.0f;
            static constexpr float          HIGH_LEVEL_MAX      = 1.0f;
            static constexpr float          HIGH_LEVEL_DFL      = 0.5f;
            static constexpr float          HIGH_LEVEL_STEP     = 0.001f;

            static constexpr float          LOW_LEVEL_MIN       = 0.0f;
            static constexpr float          LOW_LEVEL_MAX       = 1.0f;
            static constexpr float          LOW_LEVEL_DFL       = 0.5f;
            static constexpr float          LOW_LEVEL_STEP      = 0.001f;

            static constexpr float          RADIUS_MIN          = 0.0f;
            static constexpr float          RADIUS_MAX          = 1.0f;
            static constexpr float          RADIUS_DFL          = 0.5f;
            static constexpr float          RADIUS_STEP         = 0.001f;

            static constexpr float          LEVELS_MIN          = 0.0f;
            static constexpr float          LEVELS_MAX          = 1.0f;
            static constexpr float          LEVELS_DFL          = 0.5f;
            static constexpr float          LEVELS_STEP         = 0.001f;

            static constexpr float          C_COMPANDING_MIN    = 0.0f;
            static constexpr float          C_COMPANDING_MAX    = 1.0f;
            static constexpr float          C_COMPANDING_DFL    = 0.5f;
            static constexpr float          C_COMPANDING_STEP   = 0.001f;

            static constexpr float          Q_COMPANDING_MIN    = 0.0f;
            static constexpr float          Q_COMPANDING_MAX    = 1.0f;
            static constexpr float          Q_COMPANDING_DFL    = 0.5f;
            static constexpr float          Q_COMPANDING_STEP   = 0.001f;

            static constexpr float          BIAS_MIN            = 0.0f;
            static constexpr float          BIAS_MAX            = 1.0f;
            static constexpr float          BIAS_DFL            = 0.5f;
            static constexpr float          BIAS_STEP           = 0.001f;

            static constexpr float          BLEND_MIN           = 0.0f;
            static constexpr float          BLEND_MAX           = 1.0f;
            static constexpr float          BLEND_DFL           = 0.5f;
            static constexpr float          BLEND_STEP          = 0.001f;

            static constexpr float          DRIVE_MIN           = 0.0f;
            static constexpr float          DRIVE_MAX           = 1.0f;
            static constexpr float          DRIVE_DFL           = 0.5f;
            static constexpr float          DRIVE_STEP          = 0.001f;

            enum shaping_function_selector_t
            {
                SAT_SH_FCN_SINUSOIDAL,
                SAT_SH_FCN_POLYNOMIAL,
                SAT_SH_FCN_HYPERBOLIC,
                SAT_SH_FCN_EXPONENTIAL,
                SAT_SH_FCN_POWER,
                SAT_SH_FCN_BILINEAR,
                SAT_SH_FCN_ASYMMETRIC_CLIP,
                SAT_SH_FCN_ASYMMETRIC_SOFTCLIP,
                SAT_SH_FCN_QUARTER_CIRCLE,
                SAT_SH_FCN_RECTIFIER,
                SAT_SH_FCN_BITCRUSH_FLOOR,
                SAT_SH_FCN_BITCRUSH_CEIL,
                SAT_SH_FCN_BITCRUSH_ROUND,
                SAT_SH_FCN_CONTINUOUS_A_LAW_COMPRESSION,
                SAT_SH_FCN_CONTINUOUS_A_LAW_EXPANSION,
                SAT_SH_FCN_CONTINUOUS_MU_LAW_COMPRESSION,
                SAT_SH_FCN_CONTINUOUS_MU_LAW_EXPANSION,
                SAT_SH_FCN_QUANTIZED_A_LAW_COMPRESSION,
                SAT_SH_FCN_QUANTIZED_A_LAW_EXPANSION,
                SAT_SH_FCN_QUANTIZED_MU_LAW_COMPRESSION,
                SAT_SH_FCN_QUANTIZED_MU_LAW_EXPANSION,
                SAT_SH_FCN_TAP_TUBEWARMTH,

                SAT_SH_FCN_DEFAULT = SAT_SH_FCN_HYPERBOLIC
            };

            static const float band_frequencies_x3[];
            static const float band_frequencies_x8[];
            static const float band_frequencies_x16[];
            static const float band_frequencies_x32[];

        } saturator;

        // Plugin type metadata
        extern const meta::plugin_t saturator_x3_mono;
        extern const meta::plugin_t saturator_x3_stereo;
        extern const meta::plugin_t saturator_x8_mono;
        extern const meta::plugin_t saturator_x8_stereo;
        extern const meta::plugin_t saturator_x16_mono;
        extern const meta::plugin_t saturator_x16_stereo;
        extern const meta::plugin_t saturator_x32_mono;
        extern const meta::plugin_t saturator_x32_stereo;

    } /* namespace meta */
} /* namespace lsp */

#endif /* PRIVATE_META_SATURATOR_H_ */
