#ifndef VIRTUALKEYBOARDINPUTMETHOD_H
#define VIRTUALKEYBOARDINPUTMETHOD_H

#include <wpe/webkit.h>


/// This is a custom modifier we add to keyboard events injected by the virtual
/// keyboard.  It is used so we can filter the keys when the virtual keyboard
/// is shown.
#define wpe_input_keyboard_modifier_virtual_keyboard  uint32_t(1 << 31)


G_BEGIN_DECLS

#define VK_TYPE_INPUT_METHOD_CONTEXT            (virtual_keyboard_input_method_context_get_type())
#define VK_INPUT_METHOD_CONTEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), VK_TYPE_INPUT_METHOD_CONTEXT, VirtualKeyboardInputMethodContext))
#define VK_INPUT_METHOD_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  VK_TYPE_INPUT_METHOD_CONTEXT, VirtualKeyboardInputMethodContextClass))
#define VK_IS_INPUT_METHOD_CONTEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), VK_TYPE_INPUT_METHOD_CONTEXT))
#define VK_IS_INPUT_METHOD_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  VK_TYPE_INPUT_METHOD_CONTEXT))
#define VK_INPUT_METHOD_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  VK_TYPE_INPUT_METHOD_CONTEXT, VirtualKeyboardInputMethodContextClass))

typedef struct _VirtualKeyboardInputMethodContext
{
    WebKitInputMethodContext parent;

    /*< private >*/

} VirtualKeyboardInputMethodContext;

typedef struct _VirtualKeyboardInputMethodContextClass
{
    WebKitInputMethodContextClass parent;

    /* Signals */
    void     (* show_keyboard)    (WebKitInputMethodContext        *context);
    void     (* hide_keyboard)    (WebKitInputMethodContext        *context);


} VirtualKeyboardInputMethodContextClass;

GType virtual_keyboard_input_method_context_get_type(void);

G_END_DECLS

#endif
