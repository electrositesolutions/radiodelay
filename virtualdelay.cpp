#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Light_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/fl_ask.H>
#include <string.h>
#include <stdlib.h>

#define MA_DEBUG_OUTPUT
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// Define basic global structure variables
Fl_Double_Window *window_main = NULL;
Fl_Light_Button *button_play = NULL;

static void stop() {
    if (button_play) {
        button_play->clear();
        button_play->label("Play (F5)");
    }
}

// ⚠️ INTERACTIVE POP-UP WARNING DIALOG FOR PLAY/STOP ACTION
static void cb_play_toggle(Fl_Widget* widget, void* userdata) {
    Fl_Light_Button* btn = (Fl_Light_Button*)widget;
    
    if (btn->value() == 0) {
        // Intercept action with a confirmation pop-up modal dialog box
        int confirm = fl_choice("Are you sure you want to stop virtual delay audio streaming?", 
                                "Cancel", "Stop Audio", NULL);
        
        if (confirm == 0) { 
            btn->value(1); // Force state visual back on if user hit cancel
            return;
        }
        stop(); // Proceed to execute internal stop cleanup if confirmed
    } else {
        btn->label("Stop (F5)");
    }
}

int arg_handler(int argc, char **argv, int &i) {
    return 0;
}

// Complete main entry execution flow
int main(int argc, char **argv) {
    Fl::args(argc, argv, arg_handler);
    
    // Natively build the layout window instead of using an external generator function
    window_main = new Fl_Double_Window(340, 180, "Virtual Delay");
    
    button_play = new Fl_Light_Button(20, 60, 300, 50, "Play (F5)");
    button_play->callback(cb_play_toggle);
    
    window_main->end();
    window_main->show(argc, argv);
    
    return Fl::run();
}
