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

#include <lsp-plug.in/plug-fw/meta/ports.h>
#include <lsp-plug.in/shared/meta/developers.h>
#include <private/meta/saturator.h>

#define LSP_PLUGINS_SATURATOR_VERSION_MAJOR       1
#define LSP_PLUGINS_SATURATOR_VERSION_MINOR       0
#define LSP_PLUGINS_SATURATOR_VERSION_MICRO       0

#define LSP_PLUGINS_SATURATOR_VERSION  \
    LSP_MODULE_VERSION( \
        LSP_PLUGINS_SATURATOR_VERSION_MAJOR, \
        LSP_PLUGINS_SATURATOR_VERSION_MINOR, \
        LSP_PLUGINS_SATURATOR_VERSION_MICRO  \
    )

namespace lsp
{
    namespace meta
    {
        //-------------------------------------------------------------------------
        // Plugin metadata

        // NOTE: Port identifiers should not be longer than 7 characters as it will overflow VST2 parameter name buffers
        static const port_t saturator_mono_ports[] =
        {
            // Input and output audio ports
            PORTS_MONO_PLUGIN,

            // Input controls
            BYPASS,
            INT_CONTROL("d_in", "Delay in samples", "Delay", U_SAMPLES, saturator::SAMPLES),
            DRY_GAIN(0.0f),
            WET_GAIN(1.0f),
            OUT_GAIN,
            OPT_STRING("comment", "Comment", 128),

            // Output controls
            METER_MINMAX("d_out", "Delay time in milliseconds", U_MSEC, 0.0f, saturator::DELAY_OUT_MAX_TIME),
            METER_GAIN("min", "Input gain", GAIN_AMP_P_48_DB),
            METER_GAIN("mout", "Output gain", GAIN_AMP_P_48_DB),

            PORTS_END
        };

        // NOTE: Port identifiers should not be longer than 7 characters as it will overflow VST2 parameter name buffers
        static const port_t saturator_stereo_ports[] =
        {
            // Input and output audio ports
            PORTS_STEREO_PLUGIN,

            // Input controls
            BYPASS,
            INT_CONTROL("d_in", "Delay in samples", "Delay", U_SAMPLES, saturator::SAMPLES),
            DRY_GAIN(0.0f),
            WET_GAIN(1.0f),
            OUT_GAIN,
            OPT_STRING("comment", "Comment", 128),

            // Output controls
            METER_MINMAX("d_out", "Delay time in milliseconds", U_MSEC, 0.0f, saturator::DELAY_OUT_MAX_TIME),
            METER_GAIN("min_l", "Input gain left",  GAIN_AMP_P_48_DB),
            METER_GAIN("mout_l", "Output gain left",  GAIN_AMP_P_48_DB),
            METER_GAIN("min_r", "Input gain right",  GAIN_AMP_P_48_DB),
            METER_GAIN("mout_r", "Output gain right", GAIN_AMP_P_48_DB),

            PORTS_END
        };

        static const int plugin_classes[]       = { C_UTILITY, -1 };
        static const int clap_features_mono[]   = { CF_AUDIO_EFFECT, CF_UTILITY, CF_MONO, -1 };
        static const int clap_features_stereo[] = { CF_AUDIO_EFFECT, CF_UTILITY, CF_STEREO, -1 };

        const meta::bundle_t saturator_bundle =
        {
            "saturator", // TODO: write proper bundle identifier
            "Plugin Template", // TODO: write proper bundle name
            B_UTILITIES,
            "", // TODO: provide ID of the video on YouTube
            "Plugin Template" // TODO: write plugin description, should be the same to the english version in 'bundles.json'
        };

        const plugin_t saturator_mono =
        {
            "Pluginschablone Mono",
            "Plugin Template Mono",
            "Plugin Template Mono",
            "PS1M",
            &developers::v_sadovnikov,
            "saturator_mono",
            {
                LSP_LV2_URI("saturator_mono"),
                LSP_LV2UI_URI("saturator_mono"),
                "xxxx",         // TODO: fill valid VST2 ID (4 letters/digits)
                LSP_VST3_UID("ps1m    xxxx"),
                LSP_VST3UI_UID("ps1m    xxxx"),
                1,              // TODO: fill valid LADSPA identifier (positive decimal integer)
                LSP_LADSPA_URI("saturator_mono"),
                LSP_CLAP_URI("saturator_mono"),
                LSP_GST_UID("saturator_mono"),
            },
            LSP_PLUGINS_SATURATOR_VERSION,
            plugin_classes,
            clap_features_mono,
            E_DUMP_STATE,
            saturator_mono_ports,
            "template/plugin.xml",
            NULL,
            mono_plugin_port_groups,
            &saturator_bundle
        };

        const plugin_t saturator_stereo =
        {
            "Pluginschablone Stereo",
            "Plugin Template Stereo",
            "Plugin Template Stereo",
            "PS1S",
            &developers::v_sadovnikov,
            "saturator_stereo",
            {
                LSP_LV2_URI("saturator_stereo"),
                LSP_LV2UI_URI("saturator_stereo"),
                "yyyy",         // TODO: fill valid VST2 ID (4 letters/digits)
                LSP_VST3_UID("ps1s    yyyy"),
                LSP_VST3UI_UID("ps1s    yyyy"),
                2,              // TODO: fill valid LADSPA identifier (positive decimal integer)
                LSP_LADSPA_URI("saturator_stereo"),
                LSP_CLAP_URI("saturator_stereo"),
                LSP_GST_UID("saturator_stereo"),
            },
            LSP_PLUGINS_SATURATOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE,
            saturator_stereo_ports,
            "template/plugin.xml",
            NULL,
            stereo_plugin_port_groups,
            &saturator_bundle
        };
    } /* namespace meta */
} /* namespace lsp */



