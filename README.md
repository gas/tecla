# Tecla

Tecla is a keyboard layout viewer.

Tecla uses GTK/Libadwaita for UI, and libxkbcommon to deal with keyboard maps.

## Características en este Fork

Esta versión de Tecla incluye las siguientes mejoras y características adicionales:

### 1. Visualización Mejorada de Teclas

* **Doble Etiqueta por Tecla:** Cada tecla ahora puede mostrar dos caracteres simultáneamente:
    * El carácter principal (activado por la tecla base o en combinación con Shift) se muestra con el color estándar.
    * El carácter secundario (activado por AltGr o AltGr+Shift) se muestra en una posición diferente y en color rojo, permitiendo una visualización más clara de distribuciones complejas como EurKEY.
* **Representación Física ANSI (Opcional):**
    * (Si has completado esta parte y es fácilmente configurable o por defecto en tu fork) Se utiliza una representación de teclado físico ANSI de 104 teclas (`ansi104.h`) para una visualización más precisa en teclados con esta disposición.

### 2. Opciones de Línea de Comandos

Se han añadido las siguientes opciones de línea de comandos para un mayor control sobre la aplicación:

* **`--size <TAMAÑO>`** o **`-s <TAMAÑO>`**:
    Establece el tamaño de la ventana. Los valores posibles para `<TAMAÑO>` son:
    * `small`: Ventana pequeña.
    * `normal`: Ventana de tamaño normal (predeterminado si no se especifica).
    * `large`: Ventana grande.
* **`--geometry <ANCHOxALTO>`** o **`-g <ANCHOxALTO>`**:
    Establece unas dimensiones específicas para la ventana. Por ejemplo: `--geometry=750x500`.
* **`[LAYOUT]`**:
    Puedes especificar un nombre de layout (ej. `es`, `us`, `de+eurkey`) como argumento posicional para cargar una distribución específica al inicio.

**Ejemplos de uso:**

```bash
# Mostrar Tecla con tamaño pequeño
tecla --size=small

# Mostrar Tecla con dimensiones específicas y layout español
tecla es --geometry=700x450