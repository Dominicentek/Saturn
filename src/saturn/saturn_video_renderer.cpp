#include "saturn_video_renderer.h"

#include <cstring>
#include <iostream>
#include <filesystem>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include "pc/pngutils.h"
}

void pngseq_finalize() {}

int video_width;
int video_height;
int png_counter;

std::string output;

FILE* ffmpeg;

void pngseq_init(int w, int h, bool fps60) {
    video_width = w;
    video_height = h;
    png_counter = 0;
}

void pngseq_render(unsigned char* data) {
    std::string file = output.substr(0, output.find_last_of("."));
    pngutils_write_png((file + std::to_string(++png_counter) + ".png").c_str(), video_width, video_height, 4, data, 0);
}

void webm_init(int w, int h, bool fps60) {
    video_width = w;
    video_height = h;
    std::string cmd = "ffmpeg -y -r " + std::string(fps60 ? "60" : "30") + " -f rawvideo -pix_fmt rgba -s " + std::to_string(w) + "x" + std::to_string(h) + " -i - -c:v libvpx-vp9 -pix_fmt yuva420p \"" + output + "\"";
#ifdef _WIN32
    ffmpeg = popen(cmd.c_str(), "wb");
#else
    ffmpeg = popen(cmd.c_str(), "w");
#endif
}

void mp4_init(int w, int h, bool fps60) {
    video_width = w;
    video_height = h;
    std::string cmd = "ffmpeg -y -r " + std::string(fps60 ? "60" : "30") + " -f rawvideo -pix_fmt rgba -s " + std::to_string(w) + "x" + std::to_string(h) + " -i - -c:v h264 -pix_fmt yuv420p \"" + output + "\"";
#ifdef _WIN32
    ffmpeg = popen(cmd.c_str(), "wb");
#else
    ffmpeg = popen(cmd.c_str(), "w");
#endif
}

void gif_init(int w, int h, bool fps60) {
    video_width = w;
    video_height = h;
    std::string cmd = "ffmpeg -y -r 30 -f rawvideo -pix_fmt rgba -s " + std::to_string(w) + "x" + std::to_string(h) + " -i -  -vf \"format=rgba,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse\" \"" + output + "\"";
#ifdef _WIN32
    ffmpeg = popen(cmd.c_str(), "wb");
#else
    ffmpeg = popen(cmd.c_str(), "w");
#endif
}

void mov_init(int w, int h, bool fps60) {
    video_width = w;
    video_height = h;
    std::string cmd = "ffmpeg -y -r " + std::string(fps60 ? "60" : "30") + " -f rawvideo -pix_fmt rgba -s " + std::to_string(w) + "x" + std::to_string(h) + " -i - -c:v qtrle -pix_fmt argb \"" + output + "\"";
#ifdef _WIN32
    ffmpeg = popen(cmd.c_str(), "wb");
#else
    ffmpeg = popen(cmd.c_str(), "w");
#endif
}

void ffmpeg_render(unsigned char* data) {
    fwrite(data, video_width * video_height * 4, 1, ffmpeg);
}

void ffmpeg_finalize() {
    fclose(ffmpeg);
}

VideoRenderer renderer_pngseq = {
    pngseq_init,
    pngseq_render,
    pngseq_finalize,
    VIDEO_RENDERER_FLAGS_TRANSPARECY | VIDEO_RENDERER_FLAGS_60FPS,
};

VideoRenderer renderer_webm = {
    webm_init,
    ffmpeg_render,
    ffmpeg_finalize,
    VIDEO_RENDERER_FLAGS_FFMPEG | VIDEO_RENDERER_FLAGS_TRANSPARECY | VIDEO_RENDERER_FLAGS_60FPS,
};

VideoRenderer renderer_mp4 = {
    mp4_init,
    ffmpeg_render,
    ffmpeg_finalize,
    VIDEO_RENDERER_FLAGS_FFMPEG | VIDEO_RENDERER_FLAGS_60FPS,
};

VideoRenderer renderer_gif = {
    gif_init,
    ffmpeg_render,
    ffmpeg_finalize,
    VIDEO_RENDERER_FLAGS_FFMPEG | VIDEO_RENDERER_FLAGS_TRANSPARECY,
};

VideoRenderer renderer_mov = {
    mov_init,
    ffmpeg_render,
    ffmpeg_finalize,
    VIDEO_RENDERER_FLAGS_FFMPEG | VIDEO_RENDERER_FLAGS_TRANSPARECY | VIDEO_RENDERER_FLAGS_60FPS,
};

std::vector<std::pair<std::pair<std::string, std::string>, VideoRenderer>> video_renderers =  {
    { { "png",  "PNG sequence"     }, renderer_pngseq },
    { { "webm", "Transparent WEBM" }, renderer_webm   },
    { { "mp4",  "MPEG-4 video"     }, renderer_mp4    },
    { { "gif",  "GIF animation"    }, renderer_gif    },
    { { "mov",  "Transparent MOV"  }, renderer_mov    },
};

FUNC(video_renderer_init      FUNC_INIT    ) = nullptr;
FUNC(video_renderer_render    FUNC_RENDER  ) = nullptr;
FUNC(video_renderer_finalize  FUNC_FINALIZE) = nullptr;
int  video_renderer_flags                    = VIDEO_RENDERER_FLAGS_NONE;

void saturn_set_video_destination(std::string filename) {
    int index = -1;
    std::string ext = std::filesystem::path(filename).extension().string();
    for (int i = 0; i < video_renderers.size(); i++) {
        auto renderer = video_renderers[i];
        if ("." + renderer.first.first == ext) {
            index = i;
            break;
        }
    }
    if (index == -1) return;
    VideoRenderer video_renderer = video_renderers[index].second;
    video_renderer_init     = std::get<0>(video_renderer);
    video_renderer_render   = std::get<1>(video_renderer);
    video_renderer_finalize = std::get<2>(video_renderer);
    video_renderer_flags    = std::get<3>(video_renderer);
    output = filename;
}

std::vector<std::string> video_renderer_get_formats(bool ffmpeg) {
    std::vector<std::string> formats = {};
    if (ffmpeg) for (int i = 0; i < video_renderers.size(); i++) {
        if (!(std::get<3>(video_renderers[i].second) & VIDEO_RENDERER_FLAGS_FFMPEG)) continue;
        formats.push_back("." + video_renderers[i].first.first);
    }
    for (int i = 0; i < video_renderers.size(); i++) {
        if (std::get<3>(video_renderers[i].second) & VIDEO_RENDERER_FLAGS_FFMPEG) continue;
        formats.push_back("." + video_renderers[i].first.first);
    }
    return formats;
}
