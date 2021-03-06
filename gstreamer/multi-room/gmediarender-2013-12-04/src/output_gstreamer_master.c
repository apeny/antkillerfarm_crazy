#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gst/gst.h>

#include "logging.h"
#include "upnp_control_point.h"
#include "output_module.h"
#include "output_gstreamer.h"

static void master_pad_added_handler (GstElement *src, GstPad *new_pad, gpointer data)
{
	GstPad *sink_pad = gst_element_get_static_pad (gst_data.convert, "sink");
	GstPadLinkReturn ret;
	GstCaps *new_pad_caps = NULL;
	GstStructure *new_pad_struct = NULL;
	const gchar *new_pad_type = NULL;
	guint caps_size = 0, i;

	g_print ("Received new pad '%s' from '%s':\n", GST_PAD_NAME (new_pad), GST_ELEMENT_NAME (src));
	g_print ("sink_pad: '%s'\n", GST_PAD_NAME (sink_pad));

	if (gst_pad_is_linked (sink_pad)) {
		g_print ("We are already linked. Ignoring.\n");
		goto exit;
	}

	new_pad_caps = gst_pad_get_current_caps(new_pad);
	caps_size = gst_caps_get_size(new_pad_caps);
	g_print ("caps_size : %d\n", caps_size);
	for (i = 0; i < caps_size; i++)
	{
		new_pad_struct = gst_caps_get_structure(new_pad_caps, i);
		new_pad_type = gst_structure_get_name(new_pad_struct);
		g_print ("new_pad_type %d: '%s'\n", i, new_pad_type);
		if (strstr(new_pad_type, "audio/x-raw"))
		{
			ret = gst_pad_link (new_pad, sink_pad);
			if (GST_PAD_LINK_FAILED (ret)) {
				g_print ("Type is '%s' but link failed.\n", new_pad_type);
			} else {
				g_print ("Link succeeded (type '%s').\n", new_pad_type);
			}
			break;
		}
	}

exit:
	/* Unreference the new pad's caps, if we got them */
	if (new_pad_caps != NULL)
		gst_caps_unref (new_pad_caps);
   
	/* Unreference the sink pad */
	gst_object_unref (sink_pad);
}

# if 0
int add_slave_to_pipeline(char* ip_addr)
{
	return 0;
}

int output_gstreamer_init_master(void)
{
	GstElement *source;
	GstElement *rtppay;
	GstElement *decode_bin;

	player_ = gst_pipeline_new("audio_player_master");
	source = gst_element_factory_make ("uridecodebin", "source");
	rtppay = gst_element_factory_make ("rtpgstpay", "rtppay");
	decode_bin = gst_element_factory_make ("decodebin", "decode_bin");
        gst_data.udp_sink = gst_element_factory_make ("udpsink", "udp_sink");

	if (!player_ || !source || !rtppay || !decode_bin || !gst_data.udp_sink)
	{
		g_print ("Not all elements could be created.\n");
	}

	gst_bin_add_many (GST_BIN (player_), source, rtppay, decode_bin, gst_data.udp_sink, NULL);
	
	g_signal_connect (player_, "pad-added", G_CALLBACK (master_pad_added_handler), NULL);
	return 0;
}
#endif

# if 1
int add_slave_to_pipeline(char* ip_addr)
{
	GstElement *queue1;
	GstElement *tcp_sink;
        queue1 = gst_element_factory_make ("queue", "queue1");
	tcp_sink = gst_element_factory_make ("tcpclientsink", "tcp_sink");

	gst_bin_add_many (GST_BIN (player_), queue1, tcp_sink, NULL);
	
	if (gst_element_link_many (gst_data.tee, queue1, tcp_sink, NULL) != TRUE)
	{
		g_print ("Elements could not be linked. 2\n");
		//gst_object_unref (player_);
	}

	g_object_set (tcp_sink, "host", ip_addr, NULL);
	g_print ("add_slave_to_pipeline %s\n", ip_addr);
	g_object_set (tcp_sink, "port", MEDIA_PORT, NULL);
	return 0;
}

int output_gstreamer_init_master(void)
{
	GstBus *bus;
	GstElement *queue0;
	GstElement *decode_bin;
	GstElement *audio_sink0;

	player_ = gst_pipeline_new("audio_player_master");
	gst_data.source = gst_element_factory_make ("souphttpsrc", "source");
	gst_data.tee = gst_element_factory_make ("tee", "tee");
	queue0 = gst_element_factory_make ("queue", "queue");
	decode_bin = gst_element_factory_make ("decodebin", "decode_bin");
	gst_data.convert = gst_element_factory_make("audioconvert", "convert");
        audio_sink0 = gst_element_factory_make ("autoaudiosink", "audio_sink");

	if (!player_ || !gst_data.source || !gst_data.tee || !queue0 || !decode_bin || !gst_data.convert || !audio_sink0)
	{
		g_print ("Not all elements could be created.\n");
	}

	gst_bin_add_many (GST_BIN (player_), gst_data.source, gst_data.tee, queue0, decode_bin, gst_data.convert, audio_sink0, NULL);

	if (gst_element_link_many (gst_data.source, gst_data.tee, NULL) != TRUE)
	{
		g_print ("Elements could not be linked. 1\n");
		//gst_object_unref (player_);
	}

	if (gst_element_link_many (gst_data.tee, queue0, decode_bin, NULL) != TRUE)
	{
		g_print ("Elements could not be linked. 2\n");
		//gst_object_unref (player_);
	}

	if (gst_element_link_many (gst_data.convert, audio_sink0, NULL) != TRUE)
	{
		g_print ("Elements could not be linked. 3\n");
		//gst_object_unref (player_);
	}
	
	g_signal_connect (decode_bin, "pad-added", G_CALLBACK (master_pad_added_handler), NULL);

	bus = gst_pipeline_get_bus(GST_PIPELINE(player_));
	gst_bus_add_watch(bus, my_bus_callback, NULL);
	gst_object_unref(bus);

	if (audio_sink != NULL) {
		GstElement *sink = NULL;
		Log_info("gstreamer", "Setting audio sink to %s; device=%s\n",
			 audio_sink, audio_device ? audio_device : "");
		sink = gst_element_factory_make (audio_sink, "sink");
		if (sink == NULL) {
		  Log_error("gstreamer", "Couldn't create sink '%s'",
			    audio_sink);
		} else {
		  if (audio_device != NULL) {
		    g_object_set (G_OBJECT(sink), "device", audio_device, NULL);
		  }
		  g_object_set (G_OBJECT (player_), "audio-sink", sink, NULL);
		}
	}
	if (videosink != NULL) {
		GstElement *sink = NULL;
		Log_info("gstreamer", "Setting video sink to %s", videosink);
		sink = gst_element_factory_make (videosink, "sink");
		g_object_set (G_OBJECT (player_), "video-sink", sink, NULL);
	}

	if (gst_element_set_state(player_, GST_STATE_READY) ==
	    GST_STATE_CHANGE_FAILURE) {
		Log_error("gstreamer", "Error: pipeline doesn't become ready.");
	}
	return 0;
}
#endif

int add_slave_to_control(struct UpDeviceNode *devnode)
{
	GError * error = NULL;
	g_print ("%s\n", __FUNCTION__);

	devnode->user_data.client = g_socket_client_new();
	devnode->user_data.connection = g_socket_client_connect_to_host (devnode->user_data.client, devnode->user_data.ip_addr, CONTROL_PORT, NULL, &error);
	if (error != NULL)
	{
		g_print ("%s\n", error->message);
	}
	else
	{
		g_print ("Connect to %s is successful!\n", devnode->user_data.ip_addr);
	}
	return 0;
}

