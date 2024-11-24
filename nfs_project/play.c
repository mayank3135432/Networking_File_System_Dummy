#include <stdio.h>
#include <stdlib.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>

#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

void decode_audio_file(const char *filename) {
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVCodec *codec = NULL;
    AVPacket pkt;
    AVFrame *frame = NULL;
    int audio_stream_index = -1;
    int ret;

    // av_register_all() is deprecated and no longer needed in recent FFmpeg versions

    // Open input file
    if (avformat_open_input(&fmt_ctx, filename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", filename);
        exit(1);
    }

    // Retrieve stream information
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        exit(1);
    }

    // Find the first audio stream
    for (int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index = i;
            break;
        }
    }

    if (audio_stream_index == -1) {
        fprintf(stderr, "Could not find audio stream in the input file\n");
        exit(1);
    }

    // Find the decoder for the audio stream
    codec = (AVCodec *)avcodec_find_decoder(fmt_ctx->streams[audio_stream_index]->codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }

    // Allocate a codec context for the decoder
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        exit(1);
    }

    // Copy codec parameters from input stream to output codec context
    if (avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[audio_stream_index]->codecpar) < 0) {
        fprintf(stderr, "Could not copy codec parameters\n");
        exit(1);
    }

    // Initialize the codec context to use the given codec
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

    // Allocate an AVFrame to hold the decoded audio samples
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate audio frame\n");
        exit(1);
    }

    // Initialize packet
    // av_init_packet() is deprecated, initialize packet with av_packet_alloc()
    pkt = *av_packet_alloc();
    pkt.data = NULL;
    pkt.size = 0;

    // Read frames from the file
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        if (pkt.stream_index == audio_stream_index) {
            // Decode audio frame
            ret = avcodec_send_packet(codec_ctx, &pkt);
            if (ret < 0) {
                fprintf(stderr, "Error sending a packet for decoding\n");
                break;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    fprintf(stderr, "Error during decoding\n");
                    exit(1);
                }

                // Process the decoded audio frame (e.g., play it, save it, etc.)
                printf("Decoded audio frame with %d samples\n", frame->nb_samples);
            }
        }
        av_packet_unref(&pkt);
    }

    // Clean up
    av_frame_free(&frame);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <audio_file>\n", argv[0]);
        exit(1);
    }

    const char *filename = argv[1];
    decode_audio_file(filename);

    return 0;
}