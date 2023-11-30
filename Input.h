#ifndef _INPUT_H_
#define _INPUT_H_

#include "Common.h"

// NOTE(PF): These are based on Windows VKeyCodes.
enum KEY {
    KEY_M1 = 1,
    KEY_M2 = 2,
    KEY_ARROW_LEFT = 0x25,
    KEY_ARROW_UP = 0x26,
    KEY_ARROW_RIGHT = 0x27,
    KEY_ARROW_DOWN = 0x28,
    KEY_ESCAPE = 0x1B,
    KEY_SPACE = 0x20,
    KEY_ALT = 0x12,
    KEY_ENTER = 0x0D,
    KEY_A = 'A',
    KEY_B = 'B',
    KEY_C = 'C',
    KEY_D = 'D',
    KEY_E = 'E',
    KEY_F = 'F',
    KEY_G = 'G',
    KEY_H = 'H',
    KEY_I = 'I',
    KEY_J = 'J',
    KEY_K = 'K',
    KEY_L = 'L',
    KEY_M = 'M',
    KEY_N = 'N',
    KEY_O = 'O',
    KEY_P = 'P',
    KEY_Q = 'Q',
    KEY_R = 'R',
    KEY_S = 'S',
    KEY_T = 'T',
    KEY_U = 'U',
    KEY_V = 'V',
    KEY_W = 'W',
    KEY_X = 'X',
    KEY_Y = 'Y',
    KEY_Z = 'Z',
    KEY_a = 'a',
    KEY_b = 'b',
    KEY_c = 'c',
    KEY_d = 'd',
    KEY_e = 'e',
    KEY_f = 'f',
    KEY_g = 'g',
    KEY_h = 'h',
    KEY_i = 'i',
    KEY_j = 'j',
    KEY_k = 'k',
    KEY_l = 'l',
    KEY_m = 'm',
    KEY_n = 'n',
    KEY_o = 'o',
    KEY_p = 'p',
    KEY_q = 'q',
    KEY_r = 'r',
    KEY_s = 's',
    KEY_t = 't',
    KEY_u = 'u',
    KEY_v = 'v',
    KEY_w = 'w',
    KEY_x = 'x',
    KEY_y = 'y',
    KEY_z = 'z',
    KEY_K0 = '0',
    KEY_K1 = '1',
    KEY_K2 = '2',
    KEY_K3 = '3',
    KEY_K4 = '4',
    KEY_K5 = '5',
    KEY_K6 = '6',
    KEY_K7 = '7',
    KEY_K8 = '8',
    KEY_K9 = '9',
    KEY_MAX_STATES = 0xFF
};

// SINGLETON
struct Input {

    enum FRAME {
        FRAME_CURRENT = 0,
        FRAME_PREVIOUS = 1,
        FRAME_COUNT,
    };

    struct KeyState {
        bool isDown;
    };

    void Update();
    void UpdateKey(unsigned int keyCode, bool keyState);

    bool KeyPressed(unsigned int key);
    bool KeyDown(unsigned int key);
    bool KeyReleased(unsigned int key);

    KeyState      keyStates[FRAME_COUNT][KEY_MAX_STATES];
    int           activeFrame;
    static Input *Instance;
};

#endif //!_INPUT_H_