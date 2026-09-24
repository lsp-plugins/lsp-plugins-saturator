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
        static const int clap_features_lr[]         = { CF_AUDIO_EFFECT, CF_DISTORTION, CF_STEREO, -1 }; // TODO: Same as stereo?

//        static const port_item_t sat_band_slopes[] =
//        {
//            { "BT48",                   "eq.slope.bt48" },
//            { "MT48",                   "eq.slope.mt48" },
//            { "BT72",                   "eq.slope.bt72" },
//            { "MT72",                   "eq.slope.mt72" },
//            { "BT96",                   "eq.slope.bt96" },
//            { "MT96",                   "eq.slope.mt96" },
//            { NULL, NULL }
//        };
//
//        static const port_item_t sat_band_eq_modes[] =
//        {
//            { "IIR",                    "eq.type.iir" },
//            { "FIR",                    "eq.type.fir" },
//            { "FFT",                    "eq.type.fft" },
//            { "SPM",                    "eq.type.spm" },
//            { NULL, NULL }
//        };
//
//        static const port_item_t sat_band_select_3lr[] =
//        {
//            { "Bands Left",             "eq.bands_l" },
//            { "Bands Right",            "eq.bands_r" },
//            { NULL, NULL }
//        };
//
//        static const port_item_t sat_band_select_8lr[] =
//        {
//            { "Bands Left",             "eq.bands_l" },
//            { "Bands Right",            "eq.bands_r" },
//            { NULL, NULL }
//        };
//
//        static const port_item_t sat_band_select_16lr[] =
//        {
//            { "Bands Left",             "eq.bands_l" },
//            { "Bands Right",            "eq.bands_r" },
//            { NULL, NULL }
//        };
//
//        static const port_item_t sat_band_select_32[] =
//        {
//            { "Bands 1-16",             "eq.bands_1:16" },
//            { "Bands 17-32",            "eq.bands_17:32" },
//            { NULL, NULL }
//        };
//
//        static const port_item_t sat_band_select_32lr[] =
//        {
//            { "Bands Left 1-16",        "eq.bands_l_1:16" },
//            { "Bands Right 1-16",       "eq.bands_r_1:16" },
//            { "Bands Left 17-32",       "eq.bands_l_17:32" },
//            { "Bands Right 17-32",      "eq.bands_r_17:32" },
//            { NULL, NULL }
//        };

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

        static const port_item_t sat_stages[] =
        {
            {"1",           NULL },
            {"2",           NULL },
            {"3",           NULL },
            {"4",           NULL },
            {"5",           NULL },
            { NULL, NULL }
        };

        #define STAGE_VISUALOUTS(id, label) \
            MESH("vo" id, "Saturation Function" label, 2, saturator::MESH_SIZE)

        #define STAGE_BYPASS(id, label) \
            { "bp" id, "Bypass" label, NULL, U_BOOL, R_BYPASS, 0, F_UPPER | F_LOWER, 0, 1, 0, 0, NULL, NULL, NULL }

        #define STAGE_CONTROLS(id, label, alias)  \
            STAGE_BYPASS(id, label), \
            AMP_GAIN_RANGE("gn" id, "Gain" label, "Gain" alias, saturator::STAGE_GAIN_DFL, saturator::STAGE_GAIN_MIN, saturator::STAGE_GAIN_MAX), \
            AMP_GAIN_RANGE("bs" id, "Bass" label, "Bass" alias, saturator::BAND_GAIN_DFL, saturator::BAND_GAIN_MIN, saturator::BAND_GAIN_MAX), \
            AMP_GAIN_RANGE("md" id, "Mid" label, "Mid" alias, saturator::BAND_GAIN_DFL, saturator::BAND_GAIN_MIN, saturator::BAND_GAIN_MAX), \
            AMP_GAIN_RANGE("tb" id, "Treble" label, "Treble" alias, saturator::BAND_GAIN_DFL, saturator::BAND_GAIN_MIN, saturator::BAND_GAIN_MAX), \
            CONTROL("sl" id, "Slope" label, "Slope" alias, U_NONE, saturator::SLOPE), \
            CONTROL("sh" id, "Shape" label, "Shape" alias, U_NONE, saturator::SHAPE), \
            CONTROL("hl" id, "High Level" label, "High Level" alias, U_NONE, saturator::HIGH_LEVEL), \
            CONTROL("ll" id, "Low Level" label, "Low Level" alias, U_NONE, saturator::LOW_LEVEL), \
            CONTROL("rd" id, "Radius" label, "Radius" alias, U_NONE, saturator::RADIUS), \
            CONTROL("lv" id, "Levels" label, "Levels" alias, U_NONE, saturator::LEVELS), \
            CONTROL("cp" id, "Continuous Companding" label, "Cont. Comp." alias, U_NONE, saturator::C_COMPANDING), \
            CONTROL("qp" id, "Quantized Companding" label, "Quant. Comp." alias, U_NONE, saturator::Q_COMPANDING), \
            CONTROL("bs" id, "Bias" label, "Bias" alias, U_NONE, saturator::BIAS), \
            CONTROL("bl" id, "Blend" label, "Blend" alias, U_NONE, saturator::BLEND), \
            CONTROL("dv" id, "Drive" label, "Drive" alias, U_NONE, saturator::DRIVE), \
            COMBO("sp" id, "Shaping Function" label, "Shaping Fun." alias, saturator::SAT_SH_FCN_DEFAULT, sat_shaping_fcn)

        #define STAGE_SELECTOR(sat_stages) \
            COMBO("sts", "Saturator Stage Selector", "St selector", 0, sat_stages)

        #define STAGE_VISUALOUTS_MONO \
            STAGE_VISUALOUTS("1", " 1"), \
            STAGE_VISUALOUTS("2", " 2"), \
            STAGE_VISUALOUTS("3", " 3"), \
            STAGE_VISUALOUTS("4", " 4"), \
            STAGE_VISUALOUTS("5", " 5")

        #define STAGE_CONTROLS_MONO \
            STAGE_CONTROLS("1", " 1", "1"), \
            STAGE_CONTROLS("2", " 2", "2"), \
            STAGE_CONTROLS("3", " 3", "3"), \
            STAGE_CONTROLS("4", " 4", "4"), \
            STAGE_CONTROLS("5", " 5", "5")

        #define STAGE_VISUALOUTS_STEREO \
            STAGE_VISUALOUTS("1", " 1"), \
            STAGE_VISUALOUTS("2", " 2"), \
            STAGE_VISUALOUTS("3", " 3"), \
            STAGE_VISUALOUTS("4", " 4"), \
            STAGE_VISUALOUTS("5", " 5")

        #define STAGE_CONTROLS_STEREO \
            STAGE_CONTROLS("1", " 1", "1"), \
            STAGE_CONTROLS("2", " 2", "2"), \
            STAGE_CONTROLS("3", " 3", "3"), \
            STAGE_CONTROLS("4", " 4", "4"), \
            STAGE_CONTROLS("5", " 5", "5")

        #define STAGE_VISUALOUTS_LR \
            STAGE_VISUALOUTS("1l", " 1 Left"), STAGE_VISUALOUTS("1r", " 1 Right"), \
            STAGE_VISUALOUTS("2l", " 2 Left"), STAGE_VISUALOUTS("2r", " 2 Right"), \
            STAGE_VISUALOUTS("3l", " 3 Left"), STAGE_VISUALOUTS("3r", " 3 Right"), \
            STAGE_VISUALOUTS("4l", " 4 Left"), STAGE_VISUALOUTS("4r", " 4 Right"), \
            STAGE_VISUALOUTS("5l", " 5 Left"), STAGE_VISUALOUTS("5r", " 5 Right")

        #define STAGE_CONTROLS_LR \
            STAGE_CONTROLS("1l", " 1 Left", " 1L"), STAGE_CONTROLS("1r", " 1 Right", " 1R"), \
            STAGE_CONTROLS("2l", " 2 Left", " 2L"), STAGE_CONTROLS("2r", " 2 Right", " 2R"), \
            STAGE_CONTROLS("3l", " 3 Left", " 3L"), STAGE_CONTROLS("3r", " 3 Right", " 3R"), \
            STAGE_CONTROLS("4l", " 4 Left", " 4L"), STAGE_CONTROLS("4r", " 4 Right", " 4R"), \
            STAGE_CONTROLS("5l", " 5 Left", " 5L"), STAGE_CONTROLS("5r", " 5 Right", " 5R")

        #define COMMON_CONTROLS(id, label, alias) \
            COMBO("ov" id, "Oversampler Mode" label, "Ov. Mode" alias, saturator::SAT_OVS_DFL, sat_oversampler_mode), \
            AMP_GAIN_RANGE("ge" id, "Pre gain" label, "Pre gain" alias, saturator::PRE_GAIN_DFL, saturator::PRE_GAIN_MIN, saturator::PRE_GAIN_MAX), \
            AMP_GAIN_RANGE("gt" id, "Post gain" label, "Post gain" alias, saturator::POST_GAIN_DFL, saturator::POST_GAIN_MIN, saturator::POST_GAIN_MAX)

        #define COMMON_CONTROLS_MONO        COMMON_CONTROLS("", "", "")
        #define COMMON_CONTROLS_STEREO      COMMON_CONTROLS("", "", "")
        #define COMMON_CONTROLS_LR          COMMON_CONTROLS ("l", " Left", " L"), COMMON_CONTROLS ("r", " Right", " R")

        // NOTE: Port identifiers should not be longer than 7 characters as it will overflow VST2 parameter name buffers
        static const port_t saturator_mono_ports[] =
        {
            // Input and output audio ports
            PORTS_MONO_PLUGIN,

            // Input controls
            BYPASS,
            COMMON_CONTROLS_MONO,
            STAGE_SELECTOR(sat_stages),
            STAGE_CONTROLS_MONO,
            STAGE_VISUALOUTS_MONO,
            OPT_STRING("comment", "Comment", 128),

            // TODO: Output controls

            PORTS_END
        };

        // NOTE: Port identifiers should not be longer than 7 characters as it will overflow VST2 parameter name buffers
        static const port_t saturator_stereo_ports[] =
        {
            // Input and output audio ports
            PORTS_STEREO_PLUGIN,

            // Input controls
            BYPASS,
            COMMON_CONTROLS_STEREO,
            STAGE_SELECTOR(sat_stages),
            STAGE_CONTROLS_STEREO,
            STAGE_VISUALOUTS_STEREO,
            OPT_STRING("comment", "Comment", 128),

            // TODO: Output controls

            PORTS_END
        };

        // NOTE: Port identifiers should not be longer than 7 characters as it will overflow VST2 parameter name buffers
        static const port_t saturator_lr_ports[] =
        {
            // Input and output audio ports
            PORTS_STEREO_PLUGIN,

            // Input controls
            BYPASS,
            COMMON_CONTROLS_LR,
            STAGE_SELECTOR(sat_stages),
            STAGE_CONTROLS_LR,
            STAGE_VISUALOUTS_LR,
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

        const plugin_t saturator_mono =
        {
            "Sättigungssensor Mono",
            "Saturator Mono",
            "Saturator Mono",
            "STM",
            &developers::s_tronci,
            "saturator_mono",
            {
                LSP_LV2_URI("saturator_mono"),
                LSP_LV2UI_URI("saturator_mono"),
                "xxxx",         // TODO: fill valid VST2 ID (4 letters/digits)
                LSP_VST3_UID("stm    xxxx"),
                LSP_VST3UI_UID("stm    xxxx"),
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
            "plugins/effects/saturator/mono_stereo.xml",
            NULL,
            mono_plugin_port_groups,
            &saturator_bundle
        };

        const plugin_t saturator_stereo =
        {
            "Sättigungssensor Stereo",
            "Saturator Stereo",
            "Saturator Stereo",
            "STS",
            &developers::s_tronci,
            "saturator_stereo",
            {
                LSP_LV2_URI("saturator_stereo"),
                LSP_LV2UI_URI("saturator_stereo"),
                "xxxx",         // TODO: fill valid VST2 ID (4 letters/digits)
                LSP_VST3_UID("sts    xxxx"),
                LSP_VST3UI_UID("sts    xxxx"),
                1,              // TODO: fill valid LADSPA identifier (positive decimal integer)
                LSP_LADSPA_URI("saturator_stereo"),
                LSP_CLAP_URI("saturator_stereo"),
                LSP_GST_UID("saturator_stereo"),
            },
            LSP_PLUGINS_SATURATOR_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE,
            saturator_stereo_ports,
            "plugins/effects/saturator/mono_stereo.xml",
            NULL,
            mono_plugin_port_groups,
            &saturator_bundle
        };

        const plugin_t saturator_lr =
        {
            "Sättigungssensor LR",
            "Saturator LR",
            "Saturator LR",
            "STLR",
            &developers::s_tronci,
            "saturator_lr",
            {
                LSP_LV2_URI("saturator_lr"),
                LSP_LV2UI_URI("saturator_lr"),
                "xxxx",         // TODO: fill valid VST2 ID (4 letters/digits)
                LSP_VST3_UID("stlr    xxxx"),
                LSP_VST3UI_UID("stlr    xxxx"),
                1,              // TODO: fill valid LADSPA identifier (positive decimal integer)
                LSP_LADSPA_URI("saturator_lr"),
                LSP_CLAP_URI("saturator_lr"),
                LSP_GST_UID("saturator_lr"),
            },
            LSP_PLUGINS_SATURATOR_VERSION,
            plugin_classes,
            clap_features_lr,
            E_DUMP_STATE,
            saturator_lr_ports,
            "plugins/effects/saturator/lr.xml",
            NULL,
            mono_plugin_port_groups,
            &saturator_bundle
        };

    } /* namespace meta */
} /* namespace lsp */
