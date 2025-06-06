// Posible contenido para ansi104.h (simplificado)
#include "tecla-layout.h"

static TeclaLayout ansi104_layout = {
    .rows = {
        // Fila de Esc y F1-F12 (normalmente se omite en estos visualizadores o es estándar)
        // Fila de números
        { { { "TLDE" }, { "AE01" }, { "AE02" }, { "AE03" }, { "AE04" }, { "AE05" }, { "AE06" }, { "AE07" }, { "AE08" }, { "AE09" }, { "AE10" }, { "AE11" }, { "AE12" }, { "BKSP", .width = 2 }, }, },
        // Fila QWERTY
        { { { "TAB", .width = 1.5 }, { "AD01" }, { "AD02" }, { "AD03" }, { "AD04" }, { "AD05" }, { "AD06" }, { "AD07" }, { "AD08" }, { "AD09" }, { "AD10" }, { "AD11" }, { "AD12" }, { "BKSL", .width = 1.5 }, }, }, // BKSL aquí para ANSI
        // Fila ASDF
        { { { "CAPS", .width = 1.75 }, { "AC01" }, { "AC02" }, { "AC03" }, { "AC04" }, { "AC05" }, { "AC06" }, { "AC07" }, { "AC08" }, { "AC09" }, { "AC10" }, { "AC11" }, { "RTRN", .width = 2.25 }, }, }, // Enter de una sola fila y más ancho
        // Fila ZXCV
        { { { "LFSH", .width = 2.25 }, { "AB01" }, { "AB02" }, { "AB03" }, { "AB04" }, { "AB05" }, { "AB06" }, { "AB07" }, { "AB08" }, { "AB09" }, { "AB10" }, { "RTSH", .width = 2.75 }, }, }, // No hay LSGT
        // Fila inferior
        { { { "LCTL", .width = 1.25 }, { "LWIN", .width = 1.25 }, { "ALT", .width = 1.25 }, { "SPCE", .width = 6.25 }, { "RALT", .width = 1.25 }, { "RWIN", .width = 1.25 }, { "COMP", .width = 1.25 }, { "RCTL", .width = 1.25 }, }, },
    }
};
