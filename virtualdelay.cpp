#include "radiodelay.h"

// System header required for FLTK pop-up choice dialogue boxes
#include <FL/fl_ask.H>

#ifdef __linux__
#include "radiodelay.xpm"
#endif

#define MA_DEBUG_OUTPUT
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#define DEVICE_FORMAT ma_format_f32
#define DEVICE_CHANNELS 2
#define DEVICE_SAMPLE_RATE 48000
#define ABS(a) ((a) < 0 ? -(a) : a)
#define TIMER 0.01

static struct {
    bool play;
    int driver;
    int in;
    int out;
    float delay;
    const char *skipfile;
} opts = {.play = false, .driver = 0, .in = 0, .out = 0, .delay = 1.00, .skipfile = ""};

static ma_context context;
static ma_timer timer;
static bool recording;
static bool playing;
static ma_device ma_device_in;
static ma_device ma_device_out;
static ma_decoder ma_skip_decoder;
static ma_device ma_device_skip;
static ma_pcm_rb ma_pcm_rb_in;
static float amplitude_in[2];
static float amplitude_out[2];
static ma_backend enabledBackends[MA_BACKEND_COUNT];
static ma_uint32 configuredDelayFrames;
static double configuredDelaySeconds;

Fl_Choice *choice_driver = (Fl_Choice *)0;
Fl_Choice *choice_input = (Fl_Choice *)0;
Fl_Progress *progress_left_in = (Fl_Progress *)0;
Fl_Progress *progress_right_in = (Fl_Progress *)0;
Fl_Slider *slider_input_delay = (Fl_Slider *)0;
Fl_Value_Input *input_delay = (Fl_Value_Input *)0;
Fl_Progress *progress_input_delay = (Fl_Progress *)0;
Fl_Choice *choice_output = (Fl_Choice *)0;
Fl_Progress *progress_left_out = (Fl_Progress *)0;
Fl_Progress *progress_right_out = (Fl_Progress *)0;
Fl_Light_Button *button_play = (Fl_Light_Button *)0;
Fl_Light_Button *button_skip = (Fl_Light_Button *)0;
Fl_File_Input *input_skipfile = (Fl_File_Input *)0;
Fl_Window *window_about = (Fl_Window *)0;
Fl_Double_Window *window_main = (Fl_Double_Window *)0;
Fl_Button *button_mixer = (Fl_Button *)0;
Fl_Button *button_exit = (Fl_Button *)0;

static ma_uint32 calculate_delay_frames(double delaySeconds) {
    if (delaySeconds <= 0) {
        return 0;
    }

    const ma_uint64 requestedFrames = (ma_uint64)(delaySeconds * DEVICE_SAMPLE_RATE);
    const ma_uint64 bytesPerFrame = ma_get_bytes_per_frame(DEVICE_FORMAT, DEVICE_CHANNELS);
    const ma_uint64 maxRbBytes = 0x7FFFFFFFULL - (MA_SIMD_ALIGNMENT - 1);
    const ma_uint64 maxFrames = maxRbBytes / bytesPerFrame;

    if (requestedFrames > maxFrames) {
        return (ma_uint32)maxFrames;
    }

    return (ma_uint32)requestedFrames;
}

const char *get_opt(const char *name, int argc, char **argv, int i) {
    char *a = argv[i];
    if (strcmp(a, name) == 0) {
        int next = i + 1;
        if (argc > next && argv[next][0] != '-') {
            return argv[next];
        } else {
            return "1";
        }
    }
    return NULL;
}

int arg_handler(int argc, char **argv, int &i) {
    const char *play = get_opt("-play", argc, argv, i);
    if (play) {
        opts.play = true;
        i += 1;
        return 1;
    }
    const char *delay = get_opt("-delay", argc, argv, i);
    if (delay) {
        opts.delay = atof(delay);
        i += 2;
        return 2;
    }
    const char *driver = get_opt("-driver", argc, argv, i);
    if (driver) {
        opts.driver = atoi(driver);
        i += 2;
        return 2;
    }
    const char *in = get_opt("-in", argc, argv, i);
    if (in) {
        opts.in = atoi(in);
        i += 2;
        return 2;
    }
    const char *out = get_opt("-out", argc, argv, i);
    if (out) {
        opts.out = atoi(out);
        i += 2;
        return 2;
    }
    const char *skipfile = get_opt("-skipfile", argc, argv, i);
    if (skipfile) {
        opts.skipfile = skipfile;
        i += 2;
        return 2;
    }
    return 0;
}

static void cb_mixer(Fl_Widget *, void *userdata) {
#ifdef __APPLE__
    char file[] = "file:///System/Library/PreferencePanes/Sound.prefPane";
#elif WIN32
    char file[] = "file://C:/Windows/System32/SndVol.exe";
#elif __linux__
    char file[] = "/usr/bin/pavucontrol";
#endif
#ifdef __linux__
    if (system(file) == -1) {
        fl_alert("%s", file);
    }
#else
    char errmsg[512];
    if (!fl_open_uri(file, errmsg, sizeof(errmsg))) {
        fl_alert("%s", errmsg);
    }
#endif
}

static void cb_exit(Fl_Widget *, void *userdata) {
    exit(0);
}

static void cb_playback(ma_device *pDevice, void *pOutput, const void *pInput,
                        ma_uint32 frameCount) {
    ma_uint32 framesToRead = frameCount;
    ma_uint32 bytesPerFrame = ma_get_bytes_per_frame(pDevice->playback.format,
                              pDevice->playback.channels);
    void *pReadBuffer;
    ma_result result =
        ma_pcm_rb_acquire_read(&ma_pcm_rb_in, &framesToRead, &pReadBuffer);
    if (result != MA_SUCCESS) {
        return;
    }
    memcpy(pOutput, pReadBuffer, framesToRead * bytesPerFrame);
    result = ma_pcm_rb_commit_read(&ma_pcm_rb_in, framesToRead);
    if (result != MA_SUCCESS) {
        return;
    }
    ma_uint32 frame = 0;
    for (ma_uint32 i = 0; i < frameCount; i++) {
        float left = ABS(((float *)pOutput)[frame]);
        float right = ABS(((float *)pOutput)[frame + 1]);
        if (left > amplitude_out[0]) {
            amplitude_out[0] = left;
        }
        if (right > amplitude_out[1]) {
            amplitude_out[1] = right;
        }
        frame += 2;
    }
}

static void cb_capture(ma_device *pDevice, void *pOutput, const void *pInput,
                       ma_uint32 frameCount) {
    ma_uint32 framesToWrite = frameCount;
    ma_uint32 bytesPerFrame = ma_get_bytes_per_frame(pDevice->capture.format,
                              pDevice->capture.channels);
    void *pWriteBuffer;
    ma_result result =
        ma_pcm_rb_acquire_write(&ma_pcm_rb_in, &framesToWrite, &pWriteBuffer);
    if (result != MA_SUCCESS) {
        return;
    }
    memcpy(pWriteBuffer, pInput, framesToWrite * bytesPerFrame);
    result = ma_pcm_rb_commit_write(&ma_pcm_rb_in, framesToWrite);
    ma_uint32 frame = 0;
    for (ma_uint32 i = 0; i < frameCount; i++) {
        float left = ABS(((float *)pInput)[frame]);
        float right = ABS(((float *)pInput)[frame + 1]);
        if (left > amplitude_in[0]) {
            amplitude_in[0] = left;
        }
        if (right > amplitude_in[1]) {
            amplitude_in[1] = right;
        }
        frame += 2;
    }
}

static void cb_capture_stop(ma_device *pDevice) {
    progress_left_in->value(0);
    progress_right_in->value(0);
}

static void cb_playback_stop(ma_device *pDevice) {
    progress_left_out->value(0);
    progress_right_out->value(0);
}

static void stop() {
    ma_device_uninit(&ma_device_in);
    ma_device_uninit(&ma_device_out);
    ma_pcm_rb_uninit(&ma_pcm_rb_in);
    recording = false;
    progress_input_delay->value(0);
    slider_input_delay->activate();
    input_delay->activate();
    choice_driver->activate();
    choice_input->activate();
    choice_output->activate();
    button_play->clear();
    button_play->label("Play (F5)");
}

static void cb_skip_stop(ma_device *pDevice) {
    ma_decoder_uninit(&ma_skip_decoder);
}

static void skipstop() {
    ma_device_uninit(&ma_device_skip);
    ma_device_set_master_volume(&ma_device_out, 1);
}

static void skip_finished(void *userData) {
    skipstop();
    button_skip->clear();
}

void cb_skip_data(ma_device *pDevice, void *pOutput, const void *pInput,
                  ma_uint32 frameCount) {
    ma_uint64 frames = 0;
    ma_result result = ma_decoder_read_pcm_frames(&ma_skip_decoder, pOutput, frameCount, &frames);
    if (frames < frameCount) {
        Fl::awake(skip_finished, 0);
    }
}

static void skip() {
    ma_decoder_config decoder_config = ma_decoder_config_init(
                                           DEVICE_FORMAT, DEVICE_CHANNELS, DEVICE_SAMPLE_RATE);
    ma_result result = ma_decoder_init_file(input_skipfile->value(),
                                            &decoder_config, &ma_skip_decoder);
    if (result != MA_SUCCESS) {
        fl_alert("Failed to initialize skip file decoder.\n");
        return;
    }

    ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
    device_config.playback.pDeviceID = NULL;
    device_config.playback.format    = DEVICE_FORMAT;
    device_config.playback.channels  = DEVICE_CHANNELS;
    device_config.sampleRate         = DEVICE_SAMPLE_RATE;
    device_config.dataCallback       = cb_skip_data;
    device_config.stopCallback       = cb_skip_stop;

    result = ma_device_init(&context, &device_config, &ma_device_skip);
    if (result != MA_SUCCESS) {
        fl_alert("Failed to initialize skip playback device.\n");
        ma_decoder_uninit(&ma_skip_decoder);
        return;
    }

    ma_device_set_master_volume(&ma_device_out, 0.2f);
    
    if (ma_device_start(&ma_device_skip) != MA_SUCCESS) {
        fl_alert("Failed to start skip playback device.\n");
        skipstop();
    }
}

// ⚠️ NEW INTERACTIVE DIALOGUE HOOK FOR THE PLAY/STOP ACTION
static void cb_play_toggle(Fl_Widget* widget, void* userdata) {
    Fl_Light_Button* btn = (Fl_Light_Button*)widget;
    
    // User clicked button to STOP (Visual indicator turns off)
    if (btn->value() == 0) {
        // Pop open application-modal confirmation dialogue box
        int confirm = fl_choice("Are you sure you want to stop virtual delay audio streaming?", 
                                "Cancel", "Stop Audio", NULL);
        
        if (confirm == 0) { 
            btn->value(1); // Force state visual back on if user hit cancel
            return;
        }
        stop(); // Execute internal cleanup routine
    } else {
        btn->label("Stop (F5)");
    }
}

// STANDARD C++ APP ENTRY RUNTIME POINT 
int main(int argc, char **argv) {
    Fl::args(argc, argv, arg_handler);
    window_main = make_window();
    
    // Bind your new pop-up callback onto the play button component
    if(button_play) {
        button_play->callback(cb_play_toggle);
    }
    
    window_main->show(argc, argv);
    return Fl::run();
}
