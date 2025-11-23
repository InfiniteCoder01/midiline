#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <raylib.h>
#include <raymath.h>
#include <raymedia.h>

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

void SetMediaPositionAndRefresh(MediaStream *media, double timeSec) {
    SetMediaPosition(*media, timeSec);
    if (GetMediaState(*media) == MEDIA_STATE_PAUSED) {
        SetMediaState(*media, MEDIA_STATE_PLAYING);
        UpdateMediaEx(media, 0.0f);
        SetMediaState(*media, MEDIA_STATE_PAUSED);
    }    
}

unsigned char sample2bits(Color sample) {
    const int TH = 220;
    if (sample.r < TH) {
        if (sample.g > TH && sample.b < TH) return 1;
        if (sample.g < TH && sample.b > TH) return 2;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 4 || argc > 5) {
        fprintf(stderr, "Usage: %s <filename> <port> <# of white keys in the video> [offset]\n", argv[0]);
        return -1;
    }

    // Setup raylib
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(800, 450, "midiline player");
    InitAudioDevice();

    // Open video
    MediaStream video = LoadMedia(argv[1]);
    if (!IsMediaValid(video)) {
        fprintf(stderr, "Failed to load media file: %s\n", argv[1]);
        CloseAudioDevice();
        CloseWindow();
        return -1;
    }

    // Try to open serial
    int serial = open(argv[2], O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial < 0) {
        fprintf(stderr, "Failed to open port (%d): %s\n", errno, strerror(errno));
    } else {
        struct termios options;
        tcgetattr(serial, &options);

        // Set baud rate
        cfsetispeed(&options, B115200);   // input speed
        cfsetospeed(&options, B115200);   // output speed

        // 8 data bits, no parity, 1 stop bit
        options.c_cflag &= ~PARENB;
        options.c_cflag &= ~CSTOPB;
        options.c_cflag &= ~CSIZE;
        options.c_cflag |= CS8;

        // Disable hardware flow control
        options.c_cflag &= ~CRTSCTS;

        // Enable receiver, set local mode
        options.c_cflag |= (CLOCAL | CREAD);

        // Raw input/output mode
        options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        options.c_iflag &= ~(IXON | IXOFF | IXANY);
        options.c_oflag &= ~OPOST;

        // Apply settings
        tcsetattr(serial, TCSANOW, &options);

        const unsigned char reset[2] = { 255, 255 };
        (void)write(serial, reset, 2);
    }

    const int TRACK_HEIGHT = 30;
    const int total_white_keys = atoi(argv[3]);
    const int offset = argc >= 5 ? atoi(argv[4]) : 0;
    MediaProperties video_props = GetMediaProperties(video);
    unsigned char cmds[total_white_keys];
    for (int i = 0; i < total_white_keys; i++) cmds[i] = 0;

    bool debug = false;
    float speed = 1.0;
    while (!WindowShouldClose()) {
        const Vector2 viewport_size = (Vector2) { GetScreenWidth(), (GetScreenHeight() - TRACK_HEIGHT) };

        // Hotkeys & Controls
        if (IsKeyPressed(KEY_F3)) debug = !debug;
        if (IsKeyPressed(KEY_S)) speed = speed < 1.0 ? 1.0 : 0.5;
        if (IsKeyPressed(KEY_F)) speed = speed > 1.0 ? 1.0 : 2.0;

        if (IsKeyPressed(KEY_SPACE)) {
            if (GetMediaState(video) == MEDIA_STATE_PLAYING) SetMediaState(video, MEDIA_STATE_PAUSED);
            else SetMediaState(video, MEDIA_STATE_PLAYING);
        }

        if (IsKeyPressed(KEY_LEFT)) SetMediaPositionAndRefresh(&video, GetMediaPosition(video) - 5);
        if (IsKeyPressed(KEY_RIGHT)) SetMediaPositionAndRefresh(&video, GetMediaPosition(video) + 5);

        if (GetMouseY() > viewport_size.y && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            SetMediaPositionAndRefresh(&video, GetMouseX() * video_props.durationSec / viewport_size.x);
        }

        // Rendering
        UpdateMediaEx(&video, GetFrameTime() * speed);
        BeginDrawing();
            ClearBackground(BLACK);
            const float scale = fmin(
                viewport_size.x / video.videoTexture.width,
                viewport_size.y / video.videoTexture.height
            );
            Vector2 position = Vector2Scale(
                Vector2Subtract(
                    viewport_size,
                    Vector2Scale(
                        (Vector2) { video.videoTexture.width, video.videoTexture.height },
                        scale
                    )
                ),
                0.5
            );

            DrawTextureEx(video.videoTexture, position, 0.0, scale, WHITE);
            DrawRectangle(0, viewport_size.y, viewport_size.x * GetMediaPosition(video) / video_props.durationSec, TRACK_HEIGHT, BLUE);

            // Scan video and generate commands
            Image frame = LoadImageFromTexture(video.videoTexture);
            Color *colors = LoadImageColors(frame);
            const float key_width = video.videoTexture.width / (float)total_white_keys;

            for (int i = offset; i < total_white_keys; i++) {
                Vector2 sample_pos = (Vector2) { (i + 0.5) * key_width, frame.height * 0.9 };
                Color sample = colors[(int)sample_pos.y * frame.width + (int)sample_pos.x];
                Vector2 sample2_pos = (Vector2) { sample_pos.x + key_width * 0.5, frame.height * 0.8 };
                Color sample2 = colors[(int)sample2_pos.y * frame.width + (int)sample2_pos.x];

                if (debug) {
                    DrawCircleV(Vector2Add(Vector2Scale(sample_pos, scale), position), 5.0, RED);
                    DrawCircleV(Vector2Add(Vector2Scale(sample_pos, scale), position), 3.0, sample);
                    DrawCircleV(Vector2Add(Vector2Scale(sample2_pos, scale), position), 5.0, RED);
                    DrawCircleV(Vector2Add(Vector2Scale(sample2_pos, scale), position), 3.0, sample2);
                }
                
                unsigned char cmd = sample2bits(sample) | sample2bits(sample2) << 2;
                if (cmd != cmds[i] && serial >= 0) {
                    unsigned char data[2] = { cmd, i - offset };
                    (void)write(serial, data, 2);
                }
                cmds[i] = cmd;
            }

            UnloadImageColors(colors);
            UnloadImage(frame);
        EndDrawing();
    }

    if (serial >= 0) {
        const unsigned char reset[2] = { 255, 255 };
        (void)write(serial, reset, 2);
        close(serial);
    }

    UnloadMedia(&video);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
