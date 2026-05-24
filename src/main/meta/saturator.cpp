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

        static const int plugin_classes[]           = { C_DISTORTION, -1 };
        static const int clap_features_mono[]       = { CF_AUDIO_EFFECT, CF_DISTORTION, CF_MONO, -1 };
        static const int clap_features_stereo[]     = { CF_AUDIO_EFFECT, CF_DISTORTION, CF_STEREO, -1 };

        static const port_item_t sat_band_slopes[] =
        {
            { "BT48",                   "eq.slope.bt48" },
            { "MT48",                   "eq.slope.mt48" },
            { "BT72",                   "eq.slope.bt72" },
            { "MT72",                   "eq.slope.mt72" },
            { "BT96",                   "eq.slope.bt96" },
            { "MT96",                   "eq.slope.mt96" },
            { NULL, NULL }
        };

        static const port_item_t sat_band_eq_modes[] =
        {
            { "IIR",                    "eq.type.iir" },
            { "FIR",                    "eq.type.fir" },
            { "FFT",                    "eq.type.fft" },
            { "SPM",                    "eq.type.spm" },
            { NULL, NULL }
        };

        static const port_item_t sat_band_select_3lr[] =
        {
            { "Bands Left",             "eq.bands_l" },
            { "Bands Right",            "eq.bands_r" },
            { NULL, NULL }
        };

        static const port_item_t sat_band_select_8lr[] =
        {
            { "Bands Left",             "eq.bands_l" },
            { "Bands Right",            "eq.bands_r" },
            { NULL, NULL }
        };

        static const port_item_t sat_band_select_16lr[] =
        {
            { "Bands Left",             "eq.bands_l" },
            { "Bands Right",            "eq.bands_r" },
            { NULL, NULL }
        };

        static const port_item_t sat_band_select_32[] =
        {
            { "Bands 1-16",             "eq.bands_1:16" },
            { "Bands 17-32",            "eq.bands_17:32" },
            { NULL, NULL }
        };

        static const port_item_t sat_band_select_32lr[] =
        {
            { "Bands Left 1-16",        "eq.bands_l_1:16" },
            { "Bands Right 1-16",       "eq.bands_r_1:16" },
            { "Bands Left 17-32",       "eq.bands_l_17:32" },
            { "Bands Right 17-32",      "eq.bands_r_17:32" },
            { NULL, NULL }
        };

        static const port_item_t sat_oversampler_mode[] =
        {
            { "None",                   "oversampler.none" },
            { "x2",                     "oversampler.normal.x2" },
            { "x3",                     "oversampler.normal.x3" },
            { "x4",                     "oversampler.normal.x4" },
            { "x6",                     "oversampler.normal.x6" },
            { "x8",                     "oversampler.normal.x8" },
            { NULL, NULL }
        };

        static const port_item_t sat_shaping_fcn[] =
        {
            {"Sinusoidal"                       "sat.sinusoidal"},
            {"Polynomial"                       "sat.polynomial"},
            {"Hyberbolic"                       "sat.hyperbolic"},
            {"Exponential"                      "sat.exponential"},
            {"Power"                            "sat.power"},
            {"Bilinear"                         "sat.bilinear"},
            {"Asymmetric Clip"                  "sat.asymmetric_clip"},
            {"Asymmetric Softclip"              "sat.asymmetric_softclip"},
            {"Quarter Circle"                   "sat.quarter_circle"},
            {"Rectifier"                        "sat.rectifier"},
            {"Bitcrush (Floor)"                 "sat.bitcrush_floor"},
            {"Bitcrush (Ceil)"                  "sat.bitcrush_ceil"},
            {"Bitcrush (Round)"                 "sat.bitcrush_round"},
            {"Continuous A-Law Compression"     "sat.c_alaw_compression"},
            {"Continuous A-Law Expansion"       "sat.c_alaw_expansion"},
            {"Continuous u-Law Compression"     "sat.c_mulaw_compression"},
            {"Continuous u-Law Expansion"       "sat.c_mulaw_expansion"},
            {"Quantized A-Law Compression"      "sat.q_alaw_compression"},
            {"Quantized A-Law Expansion"        "sat.q_alaw_expansion"},
            {"Quantized u-Law Compression"      "sat.q_mulaw_compression"},
            {"Quantized u-Law Expansion"        "sat.q_mulaw_expansion"},
            {"TAP Tubewarmth"                   "sat.tap_tubewmarmth"},
            { NULL, NULL }
        };

        #define SATURATOR_CONTROLS(id, label, alias)  \
            COMBO("ov" id, "Oversampler Mode" label, "Oversampler Mode" alias, saturator::SAT_OVS_DFL, sat_oversampler_mode), \
            AMP_GAIN_RANGE("ge" id, "Pre gain" label, "Pre gain" alias, saturator::PRE_GAIN_DFL, saturator::PRE_GAIN_MIN, saturator::PRE_GAIN_MAX), \
            AMP_GAIN_RANGE("gt" id, "Post gain" label, "Post gain" alias, saturator::POST_GAIN_DFL, saturator::POST_GAIN_MIN, saturator::POST_GAIN_MAX), \
            CONTROL("sl" id, "Slope" label, "Slope" alias, U_NONE, saturator::SLOPE), \
            CONTROL("sh" id, "Shape" label, "Shape" alias, U_NONE, saturator::SHAPE), \
            CONTROL("hl" id, "High Level" label, "High Level" alias, U_NONE, saturator::HIGH_LEVEL), \
            CONTROL("ll" id, "Low Level" label, "Low Level" alias, U_NONE, saturator::LOW_LEVEL), \
            CONTROL("rd" id, "Radius" label, "Radius" alias, U_NONE, saturator::RADIUS), \
            CONTROL("lv" id, "Levels" label, "Levels" alias, U_NONE, saturator::LEVELS), \
            CONTROL("cp" id, "Continuous Companding" label, "Continuous Companding" alias, U_NONE, saturator::C_COMPANDING), \
            CONTROL("qp" id, "Quantized Companding" label, "Quantized Companding" alias, U_NONE, saturator::Q_COMPANDING), \
            CONTROL("bs" id, "Bias" label, "Bias" alias, U_NONE, saturator::BIAS), \
            CONTROL("bl" id, "Blend" label, "Blend" alias, U_NONE, saturator::BLEND), \
            COMBO("sp" id, "Shaping Function" label, "Shaping Function" alias, saturator::SAT_SH_FCN_DEFAULT, sat_shaping_fcn)

        #define SATURATOR_CONTROLS_MONO     SATURATOR_CONTROLS("", "", "")
        #define SATURATOR_CONTROLS_STEREO   SATURATOR_CONTROLS("", "", "")
        #define SATURATOR_CONTROLS_LR       SATURATOR_CONTROLS("l", " Left", " L"), SATURATOR_CONTROLS("r", " Right", " R")

        #define EQ_BAND(id, label, alias, x, f) \
            SWITCH("xs" id "_" #x, "Band solo" label " " f, "Solo " f alias, 0.0f), \
            SWITCH("xm" id "_" #x, "Band mute" label " " f, "Mute " f alias, 0.0f), \
            SWITCH("xe" id "_" #x, "Band on" label " " f, "On " f alias, 1.0f), \
            LOG_CONTROL("g" id "_" #x, "Band gain" label " " f, "Gain " f alias, U_GAIN_AMP, saturator::BAND_GAIN)

        #define EQ_BAND_MONO(x, f)      EQ_BAND("", "", "", x, f)
        #define EQ_BAND_STEREO(x, f)    EQ_BAND("", "", "", x, f)
        #define EQ_BAND_LR(x, f)        EQ_BAND("l", " Left", " L", x, f), EQ_BAND("r", " Right", " R", x, f)

        #define EQ_BANDS_3X(band) \
            band(0, "125"), \
            band(1, "1K"), \
            band(2, "18K")

        // Octave bands, except the first and the last https://en.wikipedia.org/wiki/Octave_band#Octave_bands.
        #define EQ_BANDS_8X(band) \
            band(0, "31.5"), \
            band(1, "63"), \
            band(2, "125"), \
            band(3, "250"), \
            band(4, "500"), \
            band(5, "1K"), \
            band(6, "4K"), \
            band(7, "8K")

        #define EQ_BANDS_16X(band) \
            band(0, "16"), \
            band(1, "25"), \
            band(2, "40"), \
            band(3, "63"), \
            band(4, "100"), \
            band(5, "160"), \
            band(6, "250"), \
            band(7, "400"), \
            band(8, "630"), \
            band(9, "1K"), \
            band(10, "1.6K"), \
            band(11, "2.5K"), \
            band(12, "4K"), \
            band(13, "6.3K"), \
            band(14, "10K"), \
            band(15, "16K")

        // Third octave bands: https://en.wikipedia.org/wiki/Octave_band#One-third_octave_bands
        #define EQ_BANDS_32X(band) \
            band(0, "16"), \
            band(1, "20"), \
            band(2, "25"), \
            band(3, "31.5"), \
            band(4, "40"), \
            band(5, "50"), \
            band(6, "63"), \
            band(7, "80"), \
            band(8, "100"), \
            band(9, "125"), \
            band(10, "160"), \
            band(11, "200"), \
            band(12, "250"), \
            band(13, "315"), \
            band(14, "400"), \
            band(15, "500"), \
            band(16, "630"), \
            band(17, "800"), \
            band(18, "1K"), \
            band(19, "1.25K"), \
            band(20, "1.6K"), \
            band(21, "2K"), \
            band(22, "2.5K"), \
            band(23, "3.15K"), \
            band(24, "4K"), \
            band(25, "5K"), \
            band(26, "6.3K"), \
            band(27, "8K"), \
            band(28, "10K"), \
            band(29, "12.5K"), \
            band(30, "16K"), \
            band(31, "20K")

        // NOTE: Port identifiers should not be longer than 7 characters as it will overflow VST2 parameter name buffers
        static const port_t saturator_x3_mono_ports[] =
        {
            // Input and output audio ports
            PORTS_MONO_PLUGIN,

            // Input controls
            BYPASS,
            SATURATOR_CONTROLS_MONO,
            EQ_BANDS_3X(EQ_BAND_MONO),
            EQ_BANDS_3X(EQ_BAND_MONO),
            OPT_STRING("comment", "Comment", 128),

            // TODO: Output controls

            PORTS_END
        };

        // NOTE: Port identifiers should not be longer than 7 characters as it will overflow VST2 parameter name buffers
        static const port_t saturator_x3_stereo_ports[] =
        {
            // Input and output audio ports
            PORTS_STEREO_PLUGIN,

            // Input controls
            BYPASS,
            SATURATOR_CONTROLS_STEREO,
            EQ_BANDS_3X(EQ_BAND_STEREO),
            EQ_BANDS_3X(EQ_BAND_STEREO),
            OPT_STRING("comment", "Comment", 128),

            // TODO: Output controls

            PORTS_END
        };

        // NOTE: Port identifiers should not be longer than 7 characters as it will overflow VST2 parameter name buffers
        static const port_t saturator_x8_mono_ports[] =
        {
            // Input and output audio ports
            PORTS_MONO_PLUGIN,

            // Input controls
            BYPASS,
            SATURATOR_CONTROLS_MONO,
            EQ_BANDS_8X(EQ_BAND_MONO),
            EQ_BANDS_8X(EQ_BAND_MONO),
            OPT_STRING("comment", "Comment", 128),

            // TODO: Output controls

            PORTS_END
        };

        // NOTE: Port identifiers should not be longer than 7 characters as it will overflow VST2 parameter name buffers
        static const port_t saturator_x8_stereo_ports[] =
        {
            // Input and output audio ports
            PORTS_STEREO_PLUGIN,

            // Input controls
            BYPASS,
            SATURATOR_CONTROLS_STEREO,
            EQ_BANDS_8X(EQ_BAND_STEREO),
            EQ_BANDS_8X(EQ_BAND_STEREO),
            OPT_STRING("comment", "Comment", 128),

            // TODO: Output controls

            PORTS_END
        };

        // NOTE: Port identifiers should not be longer than 7 characters as it will overflow VST2 parameter name buffers
        static const port_t saturator_x16_mono_ports[] =
        {
            // Input and output audio ports
            PORTS_MONO_PLUGIN,

            // Input controls
            BYPASS,
            SATURATOR_CONTROLS_MONO,
            EQ_BANDS_16X(EQ_BAND_MONO),
            EQ_BANDS_16X(EQ_BAND_MONO),
            OPT_STRING("comment", "Comment", 128),

            // TODO: Output controls

            PORTS_END
        };

        // NOTE: Port identifiers should not be longer than 7 characters as it will overflow VST2 parameter name buffers
        static const port_t saturator_x16_stereo_ports[] =
        {
            // Input and output audio ports
            PORTS_STEREO_PLUGIN,

            // Input controls
            BYPASS,
            SATURATOR_CONTROLS_STEREO,
            EQ_BANDS_16X(EQ_BAND_STEREO),
            EQ_BANDS_16X(EQ_BAND_STEREO),
            OPT_STRING("comment", "Comment", 128),

            // TODO: Output controls

            PORTS_END
        };

        // NOTE: Port identifiers should not be longer than 7 characters as it will overflow VST2 parameter name buffers
        static const port_t saturator_x32_mono_ports[] =
        {
            // Input and output audio ports
            PORTS_MONO_PLUGIN,

            // Input controls
            BYPASS,
            SATURATOR_CONTROLS_MONO,
            EQ_BANDS_32X(EQ_BAND_MONO),
            EQ_BANDS_32X(EQ_BAND_MONO),
            OPT_STRING("comment", "Comment", 128),

            // TODO: Output controls

            PORTS_END
        };

        // NOTE: Port identifiers should not be longer than 7 characters as it will overflow VST2 parameter name buffers
        static const port_t saturator_x32_stereo_ports[] =
        {
            // Input and output audio ports
            PORTS_STEREO_PLUGIN,

            // Input controls
            BYPASS,
            SATURATOR_CONTROLS_STEREO,
            EQ_BANDS_32X(EQ_BAND_STEREO),
            EQ_BANDS_32X(EQ_BAND_STEREO),
            OPT_STRING("comment", "Comment", 128),

            // TODO: Output controls

            PORTS_END
        };

        const meta::bundle_t saturator_bundle =
        {
            "saturator", // TODO: write proper bundle identifier
            "Plugin Template", // TODO: write proper bundle name
            B_EFFECTS,
            "", // TODO: provide ID of the video on YouTube
            "This plugin allows one to perform saturation of input signal, with pre- and post- EQ." // TODO: This should be the same to the english version in 'bundles.json'
        };

        const plugin_t saturator_x3_mono =
        {
            "Sättigungssensor x3 Mono",
            "Saturator x3 Mono",
            "Saturator x3 Mono",
            "ST3M",
            &developers::s_tronci,
            "saturator_x3_mono",
            {
                LSP_LV2_URI("saturator_x3_mono"),
                LSP_LV2UI_URI("saturator_x3_mono"),
                "xxxx",         // TODO: fill valid VST2 ID (4 letters/digits)
                LSP_VST3_UID("st3m    xxxx"),
                LSP_VST3UI_UID("st3m    xxxx"),
                1,              // TODO: fill valid LADSPA identifier (positive decimal integer)
                LSP_LADSPA_URI("saturator_x3_mono"),
                LSP_CLAP_URI("saturator_x3_mono"),
                LSP_GST_UID("saturator_x3_mono"),
            },
            LSP_PLUGINS_SATURATOR_VERSION,
            plugin_classes,
            clap_features_mono,
            E_DUMP_STATE,
            saturator_x3_mono_ports,
            "template/plugin.xml",
            NULL,
            mono_plugin_port_groups,
            &saturator_bundle
        };

        const plugin_t saturator_x3_stereo =
        {
            "Sättigungssensor x3 Stereo",
            "Saturator x3 Stereo",
            "Saturator x3 Stereo",
            "ST3S",
            &developers::s_tronci,
            "saturator_stereo",
            {
                LSP_LV2_URI("saturator_x3_stereo"),
                LSP_LV2UI_URI("saturator_x3_stereo"),
                "yyyy",         // TODO: fill valid VST2 ID (4 letters/digits)
                LSP_VST3_UID("st3s    yyyy"),
                LSP_VST3UI_UID("st3s    yyyy"),
                2,              // TODO: fill valid LADSPA identifier (positive decimal integer)
                LSP_LADSPA_URI("saturator_x3_stereo"),
                LSP_CLAP_URI("saturator_x3_stereo"),
                LSP_GST_UID("saturator_x3_stereo"),
            },
            LSP_PLUGINS_SATURATOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE,
            saturator_x3_stereo_ports,
            "template/plugin.xml",
            NULL,
            stereo_plugin_port_groups,
            &saturator_bundle
        };

        const plugin_t saturator_x8_mono =
        {
            "Sättigungssensor x8 Mono",
            "Saturator x8 Mono",
            "Saturator x8 Mono",
            "ST8M",
            &developers::s_tronci,
            "saturator_x8_mono",
            {
                LSP_LV2_URI("saturator_x8_mono"),
                LSP_LV2UI_URI("saturator_x8_mono"),
                "xxxx",         // TODO: fill valid VST2 ID (4 letters/digits)
                LSP_VST3_UID("st8m    xxxx"),
                LSP_VST3UI_UID("st8m    xxxx"),
                1,              // TODO: fill valid LADSPA identifier (positive decimal integer)
                LSP_LADSPA_URI("saturator_x8_mono"),
                LSP_CLAP_URI("saturator_x8_mono"),
                LSP_GST_UID("saturator_x8_mono"),
            },
            LSP_PLUGINS_SATURATOR_VERSION,
            plugin_classes,
            clap_features_mono,
            E_DUMP_STATE,
            saturator_x8_mono_ports,
            "template/plugin.xml",
            NULL,
            mono_plugin_port_groups,
            &saturator_bundle
        };

        const plugin_t saturator_x8_stereo =
        {
            "Sättigungssensor x8 Stereo",
            "Saturator x8 Stereo",
            "Saturator x8 Stereo",
            "ST3S",
            &developers::s_tronci,
            "saturator_stereo",
            {
                LSP_LV2_URI("saturator_x8_stereo"),
                LSP_LV2UI_URI("saturator_x8_stereo"),
                "yyyy",         // TODO: fill valid VST2 ID (4 letters/digits)
                LSP_VST3_UID("st8s    yyyy"),
                LSP_VST3UI_UID("st8s    yyyy"),
                2,              // TODO: fill valid LADSPA identifier (positive decimal integer)
                LSP_LADSPA_URI("saturator_x8_stereo"),
                LSP_CLAP_URI("saturator_x8_stereo"),
                LSP_GST_UID("saturator_x8_stereo"),
            },
            LSP_PLUGINS_SATURATOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE,
            saturator_x8_stereo_ports,
            "template/plugin.xml",
            NULL,
            stereo_plugin_port_groups,
            &saturator_bundle
        };

        const plugin_t saturator_x16_mono =
        {
            "Sättigungssensor x16 Mono",
            "Saturator x16 Mono",
            "Saturator x16 Mono",
            "ST16M",
            &developers::s_tronci,
            "saturator_x16_mono",
            {
                LSP_LV2_URI("saturator_x16_mono"),
                LSP_LV2UI_URI("saturator_x16_mono"),
                "xxxx",         // TODO: fill valid VST2 ID (4 letters/digits)
                LSP_VST3_UID("st16m    xxxx"),
                LSP_VST3UI_UID("st16m    xxxx"),
                1,              // TODO: fill valid LADSPA identifier (positive decimal integer)
                LSP_LADSPA_URI("saturator_x16_mono"),
                LSP_CLAP_URI("saturator_x16_mono"),
                LSP_GST_UID("saturator_x16_mono"),
            },
            LSP_PLUGINS_SATURATOR_VERSION,
            plugin_classes,
            clap_features_mono,
            E_DUMP_STATE,
            saturator_x16_mono_ports,
            "template/plugin.xml",
            NULL,
            mono_plugin_port_groups,
            &saturator_bundle
        };

        const plugin_t saturator_x16_stereo =
        {
            "Sättigungssensor x16 Stereo",
            "Saturator x16 Stereo",
            "Saturator x16 Stereo",
            "ST16S",
            &developers::s_tronci,
            "saturator_stereo",
            {
                LSP_LV2_URI("saturator_x16_stereo"),
                LSP_LV2UI_URI("saturator_x16_stereo"),
                "yyyy",         // TODO: fill valid VST2 ID (4 letters/digits)
                LSP_VST3_UID("st16s    yyyy"),
                LSP_VST3UI_UID("st16s    yyyy"),
                2,              // TODO: fill valid LADSPA identifier (positive decimal integer)
                LSP_LADSPA_URI("saturator_x16_stereo"),
                LSP_CLAP_URI("saturator_x16_stereo"),
                LSP_GST_UID("saturator_x16_stereo"),
            },
            LSP_PLUGINS_SATURATOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE,
            saturator_x16_stereo_ports,
            "template/plugin.xml",
            NULL,
            stereo_plugin_port_groups,
            &saturator_bundle
        };

        const plugin_t saturator_x32_mono =
        {
            "Sättigungssensor x32 Mono",
            "Saturator x32 Mono",
            "Saturator x32 Mono",
            "ST32M",
            &developers::s_tronci,
            "saturator_x32_mono",
            {
                LSP_LV2_URI("saturator_x32_mono"),
                LSP_LV2UI_URI("saturator_x32_mono"),
                "xxxx",         // TODO: fill valid VST2 ID (4 letters/digits)
                LSP_VST3_UID("st32m    xxxx"),
                LSP_VST3UI_UID("st32m    xxxx"),
                1,              // TODO: fill valid LADSPA identifier (positive decimal integer)
                LSP_LADSPA_URI("saturator_x32_mono"),
                LSP_CLAP_URI("saturator_x32_mono"),
                LSP_GST_UID("saturator_x32_mono"),
            },
            LSP_PLUGINS_SATURATOR_VERSION,
            plugin_classes,
            clap_features_mono,
            E_DUMP_STATE,
            saturator_x32_mono_ports,
            "template/plugin.xml",
            NULL,
            mono_plugin_port_groups,
            &saturator_bundle
        };

        const plugin_t saturator_x32_stereo =
        {
            "Sättigungssensor x32 Stereo",
            "Saturator x32 Stereo",
            "Saturator x32 Stereo",
            "ST16S",
            &developers::s_tronci,
            "saturator_stereo",
            {
                LSP_LV2_URI("saturator_x32_stereo"),
                LSP_LV2UI_URI("saturator_x32_stereo"),
                "yyyy",         // TODO: fill valid VST2 ID (4 letters/digits)
                LSP_VST3_UID("st32s    yyyy"),
                LSP_VST3UI_UID("st32s    yyyy"),
                2,              // TODO: fill valid LADSPA identifier (positive decimal integer)
                LSP_LADSPA_URI("saturator_x32_stereo"),
                LSP_CLAP_URI("saturator_x32_stereo"),
                LSP_GST_UID("saturator_x32_stereo"),
            },
            LSP_PLUGINS_SATURATOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE,
            saturator_x32_stereo_ports,
            "template/plugin.xml",
            NULL,
            stereo_plugin_port_groups,
            &saturator_bundle
        };
    } /* namespace meta */
} /* namespace lsp */



