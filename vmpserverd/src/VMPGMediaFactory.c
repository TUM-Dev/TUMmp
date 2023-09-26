/* vmp-server - A virtual multimedia processor
 * Copyright (C) 2023 Hugo Melder
 *
 * SPDX-License-Identifier: MIT
 */


#include <gst/gst.h>
#include "VMPGMediaFactory.h"

// Generated project configuration
#include "../build/config.h"

enum _VMPMediaFactoryProperty
{
    PROP_0,
    PROP_OUTPUT_CONFIGURATION,
    PROP_CAMERA_CONFIGURATION,
    PROP_PRESENTATION_CONFIGURATION,
    PROP_CAMERA_INTERPIPE_NAME,
    PROP_PRESENTATION_ITERPIPE_NAME,
    PROP_AUDIO_INTERPIPE_NAME,
    N_PROPERTIES
};

enum _VMPMediaFactoryConfigurationType
{
    VMP_MEDIA_FACTORY_OUTPUT_CONFIGURATION,
    VMP_MEDIA_FACTORY_CAMERA_CONFIGURATION,
    VMP_MEDIA_FACTORY_PRESENTATION_CONFIGURATION
};

static GParamSpec *obj_properties[N_PROPERTIES] = {
    NULL,
};

typedef struct _VMPMediaFactoryPrivate
{
    // Channel names for the intervideo and interaudio communication
    gchar *camera_interpipe_name;
    gchar *presentation_interpipe_name;
    gchar *audio_interpipe_name;

    // Configuration for the camera, and presentation sources used when composing the combined output
    VMPVideoConfig *output_configuration;
    VMPVideoConfig *camera_configuration;
    VMPVideoConfig *presentation_configuration;
} VMPMediaFactoryPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(VMPMediaFactory, vmp_media_factory, GST_TYPE_RTSP_MEDIA_FACTORY);

// Forward Declarations
GstElement *vmp_media_factory_create_element(GstRTSPMediaFactory *factory, const GstRTSPUrl *url);
static void vmp_media_factory_constructed(GObject *object);
static void vmp_media_factory_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void vmp_media_factory_add_configuration(VMPMediaFactory *bin, enum _VMPMediaFactoryConfigurationType type, VMPVideoConfig *output);
static void vmp_media_factory_finalize(GObject *object);

static void vmp_media_factory_init(VMPMediaFactory *self)
{
    // Set the media factory to be shared
    g_object_set(GST_RTSP_MEDIA_FACTORY(self), "shared", TRUE, NULL);
}

static void vmp_media_factory_class_init(VMPMediaFactoryClass *self)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(self);
    GstRTSPMediaFactoryClass *factory_class = GST_RTSP_MEDIA_FACTORY_CLASS(self);

    gobject_class->constructed = vmp_media_factory_constructed;
    gobject_class->set_property = vmp_media_factory_set_property;
    gobject_class->finalize = vmp_media_factory_finalize;
    factory_class->create_element = vmp_media_factory_create_element;

    obj_properties[PROP_CAMERA_INTERPIPE_NAME] = g_param_spec_string("camera-interpipe-name",
                                                                     "Camera Interpipe Name",
                                                                     "Name of the interpipe for the camera source",
                                                                     NULL,
                                                                     G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    obj_properties[PROP_PRESENTATION_ITERPIPE_NAME] = g_param_spec_string("presentation-interpipe-name",
                                                                          "Presentation Interpipe Name",
                                                                          "Name of the interpipe for the presentation source",
                                                                          NULL,
                                                                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    obj_properties[PROP_AUDIO_INTERPIPE_NAME] = g_param_spec_string("audio-interpipe-name",
                                                                    "Audio Interpipe Name",
                                                                    "Name of the interpipe for the audio source",
                                                                    NULL,
                                                                    G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    obj_properties[PROP_OUTPUT_CONFIGURATION] = g_param_spec_object("output-configuration",
                                                                    "Output Configuration",
                                                                    "Configuration for the combined output",
                                                                    VMP_TYPE_VIDEO_CONFIG,
                                                                    G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    obj_properties[PROP_CAMERA_CONFIGURATION] = g_param_spec_object("camera-configuration",
                                                                    "Camera Configuration",
                                                                    "Configuration for the camera source",
                                                                    VMP_TYPE_VIDEO_CONFIG,
                                                                    G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    obj_properties[PROP_PRESENTATION_CONFIGURATION] = g_param_spec_object("presentation-configuration",
                                                                          "Presentation Configuration",
                                                                          "Configuration for the presentation source",
                                                                          VMP_TYPE_VIDEO_CONFIG,
                                                                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties(gobject_class, N_PROPERTIES, obj_properties);
}

VMPMediaFactory *vmp_media_factory_new(const gchar *camera_channel, const gchar *presentation_channel, const gchar *audio_channel, VMPVideoConfig *output_configuration, VMPVideoConfig *camera_configuration, VMPVideoConfig *presentation_configuration)
{
    return g_object_new(VMP_TYPE_MEDIA_FACTORY, "camera-interpipe-name", camera_channel,
                        "presentation-interpipe-name", presentation_channel,
                        "audio-interpipe-name", audio_channel, "output-configuration", output_configuration,
                        "camera-configuration", camera_configuration, "presentation-configuration", presentation_configuration,
                        NULL);
}

static void vmp_media_factory_constructed(GObject *object)
{
    VMPMediaFactory *self = VMP_MEDIA_FACTORY(object);

    // Call the parent's constructed method
    G_OBJECT_CLASS(vmp_media_factory_parent_class)->constructed(object);

    gst_rtsp_media_factory_set_media_gtype(GST_RTSP_MEDIA_FACTORY(self), GST_TYPE_RTSP_MEDIA);
}

static void vmp_media_factory_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    VMPMediaFactory *self = VMP_MEDIA_FACTORY(object);
    VMPMediaFactoryPrivate *priv = vmp_media_factory_get_instance_private(self);

    switch (prop_id)
    {
    case PROP_OUTPUT_CONFIGURATION:
        vmp_media_factory_add_configuration(self, VMP_MEDIA_FACTORY_OUTPUT_CONFIGURATION, g_value_dup_object(value));
        break;
    case PROP_CAMERA_CONFIGURATION:
        vmp_media_factory_add_configuration(self, VMP_MEDIA_FACTORY_CAMERA_CONFIGURATION, g_value_dup_object(value));
        break;
    case PROP_PRESENTATION_CONFIGURATION:
        vmp_media_factory_add_configuration(self, VMP_MEDIA_FACTORY_PRESENTATION_CONFIGURATION, g_value_dup_object(value));
        break;
    case PROP_CAMERA_INTERPIPE_NAME:
        priv->camera_interpipe_name = g_value_dup_string(value);
        break;
    case PROP_PRESENTATION_ITERPIPE_NAME:
        priv->presentation_interpipe_name = g_value_dup_string(value);
        break;
    case PROP_AUDIO_INTERPIPE_NAME:
        priv->audio_interpipe_name = g_value_dup_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void vmp_media_factory_finalize(GObject *object)
{
    VMPMediaFactory *self;
    VMPMediaFactoryPrivate *priv;

    self = VMP_MEDIA_FACTORY(object);
    priv = vmp_media_factory_get_instance_private(self);

    g_free(priv->camera_interpipe_name);
    g_free(priv->presentation_interpipe_name);
    g_free(priv->audio_interpipe_name);
    g_object_unref(priv->output_configuration);
    g_object_unref(priv->camera_configuration);
    g_object_unref(priv->presentation_configuration);

    G_OBJECT_CLASS(vmp_media_factory_parent_class)->finalize(object);
}

/*
 * Construct the combined video bin that composites the camera and presentation sources.
 *
 * When the media_factory is set to 'shared', the element created for the first connection
 * is reused for all other connections. When no active connection is available, a new bin
 * is created.
 *
 * Here is a diagram of the combined pipeline:
 *
 * Video, and audio streams are fetched from an external pipeline via intervideosrc, and interaudiosrc
 * respectively. The capsfilter after the two videoscale elements configure the scaler according to the
 * scaling configuration supplied as a property to the factory.
 *
 * The compositor element is used to combine the two video streams into a single output. Caps negotation
 * between the two capsfilters and the compositor is done manually, after the initial pad linking is done.
 * The coordinates of the two sub-streams in the compositor is configured via the compositor's sink pads
 * (not in the simplified overview).
 *
 * After compositing, the output is configured with the capsfilter element, encoded and processed by the
 * rtp element.
 * The rtp*pay elements are responsible for encoding the video/audio into RTP packets (RFC 3984).
 *
 * To keep the audio channel synchronised, an additional queue is added between audioconvert and the
 * encoding stage.
 *
 * Please note that the simplified diagram uses the default software encoders for simplicity.
 *
 * ┌───────────────────────┐
 * │     intervideosrc     │     ┌───────┐    ┌────────────┐    ┌────────────┐    ┌────────────┐
 * │                       │────▶│ Queue │───▶│videoconvert│───▶│ videoscale │───▶│ capsfilter │────┐
 * │ channel=presentation  │     └───────┘    └────────────┘    └────────────┘    └────────────┘    │
 * └───────────────────────┘                                                                        │
 *                                                                                                  │
 * ┌───────────────────────┐                                                                        │
 * │     intervideosrc     │     ┌───────┐    ┌────────────┐    ┌────────────┐    ┌────────────┐    │
 * │                       │────▶│ Queue │───▶│videoconvert│───▶│ videoscale │───▶│ capsfilter │────┤
 * │    channel=camera     │     └───────┘    └────────────┘    └────────────┘    └────────────┘    │
 * └───────────────────────┘                                                                        │
 *                                                                                                  │
 * ┌───────────────────────┐                                                                        │
 * │     interaudiosrc     │     ┌───────┐    ┌────────────┐    ┌────────────┐    ┌────────────┐    │
 * │                       │────▶│ Queue │───▶│audioconvert│───▶│   Queue    │───▶│  voaacenc  │──┐ │
 * │     channel=audio     │     └───────┘    └────────────┘    └────────────┘    └────────────┘  │ │
 * └───────────────────────┘                                                                      │ │
 *                                                                       ┌─────────────────────┐  │ │
 *                                                                       │     rtpmp4apay      │  │ │
 *                                                                       │                     │  │ │
 *                                                                       │      name=pay1      │◀─┘ │
 *                                                                       │        pt=97        │    │
 *                                                                       └─────────────────────┘    │
 *                                                                                                  │
 * ┌───────────────────────┐                                                                        │
 * │      rtph264pay       │                                             ┌─────────────────────┐    │
 * │                       │    ┌────────────┐      ┌────────────┐       │     Compositor      │    │
 * │       name=pay0       │◀───│  x264enc   │◀─────│ capsfilter │◀──────│                     │◀───┘
 * │         pt=96         │    └────────────┘      └────────────┘       │    background=1     │
 * │                       │                                             └─────────────────────┘
 * └───────────────────────┘
 *
 * When using the Nvidia hardware encoder, the pipeline is slightly different:
 * We need to use nvvidconv to map the buffers to NVMM memory, required by the hardware encoder (nvv4l2h264enc).
 *
 * ┌───────────────────────┐
 * │      rtph264pay       │                                                          ┌─────────────────────┐
 * │                       │   ┌───────────────┐   ┌────────────┐    ┌────────────┐   │     Compositor      │
 * │       name=pay0       │◀──│ nvv4l2h264enc │◀──│ nvvidconv  │◀───│ capsfilter │◀──│                     │
 * │         pt=96         │   └───────────────┘   └────────────┘    └────────────┘   │    background=1     │
 * │                       │                                                          └─────────────────────┘
 * └───────────────────────┘
 */
GstElement *vmp_media_factory_create_element(GstRTSPMediaFactory *factory, const GstRTSPUrl *url)
{
    VMPMediaFactoryPrivate *priv;
    GstBin *bin;

    GstElement *presentation_interpipe;
    GstElement *presentation_queue;
    GstElement *presentation_videoconvert;
    GstElement *presentation_videoscale;
    GstElement *presentation_caps_filter;

    GstElement *camera_interpipe;
    GstElement *camera_queue;
    GstElement *camera_videoconvert;
    GstElement *camera_videoscale;
    GstElement *camera_caps_filter;

    GstElement *audio_interpipe;
    GstElement *audio_queue;
    GstElement *audio_audioconvert;

    GstElement *compositor;
    GstElement *compositor_caps_filter;
    GstElement *aacenc, *x264enc;
    GstElement *rtph264pay, *rtpmp4apay;

    // Nvidia Jetson specific elements
#ifdef NV_JETSON
    GstElement *nvvidconv;
#endif

    priv = vmp_media_factory_get_instance_private(VMP_MEDIA_FACTORY(factory));
    bin = GST_BIN(gst_bin_new("combined_bin"));

    // Check if source elements are present (TODO: Fail with warning)
    g_return_val_if_fail(priv->camera_interpipe_name, NULL);
    g_return_val_if_fail(priv->presentation_interpipe_name, NULL);
    g_return_val_if_fail(priv->audio_interpipe_name, NULL);
    g_return_val_if_fail(priv->output_configuration, NULL);
    g_return_val_if_fail(priv->camera_configuration, NULL);
    g_return_val_if_fail(priv->presentation_configuration, NULL);

    // Configure the presentation stream elements
    presentation_interpipe = gst_element_factory_make("intervideosrc", "presentation_interpipe");
    presentation_queue = gst_element_factory_make("queue", "presentation_queue");
    presentation_videoconvert = gst_element_factory_make("videoconvert", "presentation_videoconvert");
    presentation_videoscale = gst_element_factory_make("videoscale", "presentation_videoscale");
    presentation_caps_filter = gst_element_factory_make("capsfilter", "presentation_capsfilter");
    if (!presentation_interpipe || !presentation_queue || !presentation_videoconvert || !presentation_videoscale || !presentation_caps_filter)
    {
        GST_ERROR("Failed to create elements required for presentation stream processing");
        return NULL;
    }

    // Configure the camera stream elements
    camera_interpipe = gst_element_factory_make("intervideosrc", "camera_interpipe");
    camera_queue = gst_element_factory_make("queue", "camera_queue");
    camera_videoconvert = gst_element_factory_make("videoconvert", "camera_videoconvert");
    camera_videoscale = gst_element_factory_make("videoscale", "camera_videoscale");
    camera_caps_filter = gst_element_factory_make("capsfilter", "camera_capsfilter");
    if (!camera_interpipe || !camera_queue || !camera_videoconvert || !camera_videoscale || !camera_caps_filter)
    {
        GST_ERROR("Failed to create elements required for camera stream processing");
        return NULL;
    }

    // Configure the audio stream elements
    audio_interpipe = gst_element_factory_make("interaudiosrc", "audio_interpipe");
    audio_queue = gst_element_factory_make("queue", "audio_queue");
    audio_audioconvert = gst_element_factory_make("audioconvert", "audio_audioconvert");
    GstElement *audio_buffer = gst_element_factory_make("queue", "audio_buffer");
    if (!audio_interpipe || !audio_queue || !audio_audioconvert || !audio_buffer)
    {
        GST_ERROR("Failed to create elements required for audio stream processing");
        return NULL;
    }

    compositor = gst_element_factory_make("compositor", "compositor");
    compositor_caps_filter = gst_element_factory_make("capsfilter", "compositor_capsfilter");
    aacenc = gst_element_factory_make("avenc_aac", "aacenc");
    rtph264pay = gst_element_factory_make("rtph264pay", "pay0");
    rtpmp4apay = gst_element_factory_make("rtpmp4apay", "pay1");

    if (!compositor || !compositor_caps_filter || !aacenc || !rtph264pay || !rtpmp4apay)
    {
        GST_ERROR("Failed to create elements required for compositing and output stream processing");
        return NULL;
    }

#ifdef NV_JETSON
    // Nvidia video buffer convert
    nvvidconv = gst_element_factory_make("nvvidconv", NULL);
    // Nvidia h264 hardware encoder
    x264enc = gst_element_factory_make("nvv4l2h264enc", "x264enc");
    if (!nvvidconv || !x264enc)
    {
        GST_ERROR("Failed to create elements required for Nvidia hardware encoding");
        return NULL;
    }

    g_object_set(G_OBJECT(x264enc), "maxperf-enable", 1, NULL);
#else
    x264enc = gst_element_factory_make("x264enc", "x264enc");
    if (!x264enc)
    {
        GST_ERROR("Failed to create elements required for software encoding");
        return NULL;
    }
#endif
    g_object_set(G_OBJECT(x264enc), "bitrate", 5000000, NULL);

    // Set properties of compositor
    g_object_set(G_OBJECT(compositor), "background", 1, NULL);

    g_object_set(aacenc, "bitrate", 128000, NULL);

    /* Set properties of rtp payloader.
     * RTP Payload Format '96' is often used as the default value for H.264 video.
     * RTP Payload Format '97' is often used as the default value for AAC audio.
     */
    g_object_set(rtph264pay, "pt", 96, NULL);
    g_object_set(rtpmp4apay, "pt", 97, NULL);

    // Set properties of interpipe elements
    g_object_set(presentation_interpipe, "channel", priv->presentation_interpipe_name, NULL);
    g_object_set(camera_interpipe, "channel", priv->camera_interpipe_name, NULL);
    g_object_set(audio_interpipe, "channel", priv->audio_interpipe_name, NULL);

    /*
     * Build Caps from VMPVideoConfiguration.
     * This is used by the scaler to scale the stream accordingly.
     */
    GstCaps *compositor_caps;
    GstCaps *presentation_caps;
    GstCaps *camera_caps;

    compositor_caps = gst_caps_new_simple("video/x-raw",
                                          "width", G_TYPE_INT, vmp_video_config_get_width(priv->output_configuration),
                                          "height", G_TYPE_INT, vmp_video_config_get_height(priv->output_configuration), NULL);
    presentation_caps = gst_caps_new_simple("video/x-raw",
                                            "width", G_TYPE_INT, vmp_video_config_get_width(priv->presentation_configuration),
                                            "height", G_TYPE_INT, vmp_video_config_get_height(priv->presentation_configuration),
                                            "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1, NULL);
    camera_caps = gst_caps_new_simple("video/x-raw",
                                      "width", G_TYPE_INT, vmp_video_config_get_width(priv->camera_configuration),
                                      "height", G_TYPE_INT, vmp_video_config_get_height(priv->camera_configuration),
                                      "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1, NULL);

    g_object_set(G_OBJECT(compositor_caps_filter), "caps", compositor_caps, NULL);
    g_object_set(G_OBJECT(presentation_caps_filter), "caps", presentation_caps, NULL);
    g_object_set(G_OBJECT(camera_caps_filter), "caps", camera_caps, NULL);

    gst_caps_unref(compositor_caps);
    gst_caps_unref(presentation_caps);
    gst_caps_unref(camera_caps);

    /*
     * Add elements to the bin, and link subcomponents
     */

    // Add elements to the bin
    gst_bin_add_many(GST_BIN(bin), presentation_interpipe, presentation_queue, presentation_videoconvert, presentation_videoscale, presentation_caps_filter, NULL);
    gst_bin_add_many(GST_BIN(bin), camera_interpipe, camera_queue, camera_videoconvert, camera_videoscale, camera_caps_filter, NULL);
    gst_bin_add_many(GST_BIN(bin), compositor, compositor_caps_filter, x264enc, rtph264pay, NULL);
    gst_bin_add_many(GST_BIN(bin), audio_interpipe, audio_queue, audio_audioconvert, audio_buffer, aacenc, rtpmp4apay, NULL);

#ifdef NV_JETSON
    gst_bin_add_many(GST_BIN(bin), nvvidconv, NULL);
#endif

    // Link elements
    if (!gst_element_link_many(camera_interpipe, camera_queue, camera_videoconvert, camera_videoscale, camera_caps_filter, NULL))
    {
        GST_ERROR("Failed to link camera elements!");
        return NULL;
    }
    if (!gst_element_link_many(presentation_interpipe, presentation_queue, presentation_videoconvert, presentation_videoscale, presentation_caps_filter, NULL))
    {
        GST_ERROR("Failed to link presentation elements!");
        return NULL;
    }

#ifdef NV_JETSON
    if (!gst_element_link_many(compositor, compositor_caps_filter, nvvidconv, x264enc, rtph264pay, NULL))
    {
        GST_ERROR("Failed to link compositor elements!");
        return NULL;
    }
#else
    if (!gst_element_link_many(compositor, compositor_caps_filter, x264enc, rtph264pay, NULL))
    {
        GST_ERROR("Failed to link compositor elements!");
        return NULL;
    }
#endif

    if (!gst_element_link_many(audio_interpipe, audio_queue, audio_audioconvert, audio_buffer, aacenc, rtpmp4apay, NULL))
    {
        GST_ERROR("Failed to link audio elements!");
        return NULL;
    }

    /*
     * Pad Configuration for Compositor
     */

    GstPad *source_camera_pad;
    GstPad *sink_camera_pad;
    GstPad *source_presentation_pad;
    GstPad *sink_presentation_pad;
    GstPadTemplate *sink_template;

    // Get source pads from the source elements
    source_camera_pad = gst_element_get_static_pad(camera_caps_filter, "src");
    source_presentation_pad = gst_element_get_static_pad(presentation_caps_filter, "src");
    if (!source_camera_pad || !source_presentation_pad)
    {
        GST_ERROR("Failed to acquire source pads from camera_caps_filter or presentation_caps_filter!");
        return NULL;
    }

    // Request sink pads from the compositor
    sink_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(compositor), "sink_%u");
    sink_camera_pad = gst_element_request_pad(compositor, sink_template, NULL, NULL);
    sink_presentation_pad = gst_element_request_pad(compositor, sink_template, NULL, NULL);
    if (!sink_camera_pad || !sink_presentation_pad)
    {
        GST_ERROR("Failed to acquire sink pads from compositor!");
        return NULL;
    }

    // Configure the positions of the streams
    gint camera_pad_width = vmp_video_config_get_width(priv->output_configuration) - vmp_video_config_get_width(priv->camera_configuration);
    g_object_set(G_OBJECT(sink_camera_pad), "xpos", camera_pad_width, NULL);
    g_object_set(G_OBJECT(sink_camera_pad), "ypos", 0, NULL);

    g_object_set(G_OBJECT(sink_presentation_pad), "xpos", 0, NULL);
    g_object_set(G_OBJECT(sink_presentation_pad), "ypos", 0, NULL);

    GstPadLinkReturn link_camera_ret;
    GstPadLinkReturn link_presentation_ret;

    /* NOTE: Linking pads manually is done AFTER linking the elements automatically.
     * Otherwise the pads will be unlinked by gstreamer.
     */
    link_camera_ret = gst_pad_link(source_camera_pad, sink_camera_pad);
    if (link_camera_ret != GST_PAD_LINK_OK)
    {
        GST_ERROR("Failed to link camera source to compositor! Error: %s", gst_pad_link_get_name(link_camera_ret));
        return NULL;
    }
    link_presentation_ret = gst_pad_link(source_presentation_pad, sink_presentation_pad);
    if (link_presentation_ret != GST_PAD_LINK_OK)
    {
        GST_ERROR("Failed to link presentation source to compositor! Error: %s", gst_pad_link_get_name(link_presentation_ret));
        return NULL;
    }

    g_object_unref(source_camera_pad);
    g_object_unref(sink_camera_pad);
    g_object_unref(source_presentation_pad);
    g_object_unref(sink_presentation_pad);

    return GST_ELEMENT(bin);
}

static void vmp_media_factory_add_configuration(VMPMediaFactory *bin, enum _VMPMediaFactoryConfigurationType type, VMPVideoConfig *output)
{
    VMPMediaFactoryPrivate *priv;

    priv = vmp_media_factory_get_instance_private(bin);

    g_return_if_fail(VMP_IS_VIDEO_CONFIG(output));

    switch (type)
    {
    case VMP_MEDIA_FACTORY_OUTPUT_CONFIGURATION:
        if (priv->output_configuration)
            g_object_unref(priv->output_configuration);
        priv->output_configuration = output;
        break;
    case VMP_MEDIA_FACTORY_CAMERA_CONFIGURATION:
        if (priv->camera_configuration)
            g_object_unref(priv->camera_configuration);
        priv->camera_configuration = output;
        break;
    case VMP_MEDIA_FACTORY_PRESENTATION_CONFIGURATION:
        if (priv->presentation_configuration)
            g_object_unref(priv->presentation_configuration);
        priv->presentation_configuration = output;
        break;
    };

    g_object_ref(output);
}