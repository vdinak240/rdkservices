#include "VirtualKeyboardInputMethod.h"

//#include <QDebug>


// -----------------------------------------------------------------------------
/*!
    This is inherited glib object from the WebKitInputMethodContext gobject.
    An instance of this is registered with the web view if the virtual keyboard
    is enabled for the app, then when focusIn / focusOut virtual functions are
    called, this object emits a signal with the details.

    \see https://github.com/WebKit/WebKit/blob/b344dde25fdf17fa400dbb900aed9227b82f3ed7/Tools/TestWebKitAPI/Tests/WebKitGLib/TestInputMethodContext.cpp
    \see https://github.com/WebKit/WebKit/blob/3cc222a22d26661c615639f034c85f53014b2b85/Source/WebKit/UIProcess/API/glib/WebKitInputMethodContext.cpp
    \see https://github.com/WebKit/WebKit/blob/8afe31a018b11741abdf9b4d5bb973d7c1d9ff05/Source/WebKit/UIProcess/API/gtk/WebKitInputMethodContextImplGtk.cpp

 */


enum {
    SIGNAL_SHOW_KEYBOARD,
    SIGNAL_HIDE_KEYBOARD,

    LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0, };

enum
{
    PROP_0,
    PROP_FILTER_KEY_EVENTS,

    N_PROPERTIES
};
static GParamSpec *properties[N_PROPERTIES] = { nullptr, };



struct VirtualKeyboardInputMethodContextPrivate
{
    guint keyModifierFilter;
};


G_DEFINE_TYPE_WITH_CODE(VirtualKeyboardInputMethodContext,
                        virtual_keyboard_input_method_context,
                        WEBKIT_TYPE_INPUT_METHOD_CONTEXT,
                        G_ADD_PRIVATE(VirtualKeyboardInputMethodContext))


// -----------------------------------------------------------------------------
/*!
    \internal

    Boilerplate.

 */
static void virtualKeyboardInputMethodContextFinalize(GObject* object)
{
    G_OBJECT_CLASS(virtual_keyboard_input_method_context_parent_class)->finalize(object);
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \overload

    Called when the user has focused an HTML input element.

 */
static void virtualKeyboardInputMethodContextNotifyFocusIn(WebKitInputMethodContext* context)
{
    auto *self = reinterpret_cast<VirtualKeyboardInputMethodContext*>(context);
    auto *priv = reinterpret_cast<VirtualKeyboardInputMethodContextPrivate*>(
            virtual_keyboard_input_method_context_get_instance_private(self));

    const WebKitInputPurpose purpose = webkit_input_method_context_get_input_purpose(context);
    const WebKitInputHints hints = webkit_input_method_context_get_input_hints(context);

    /*qInfo("virtualKeyboardInputMethodContextNotifyFocusIn : purpose = %d : hints = %d",
          purpose, hints); */

    // this hint is used to suggest we don't show an on-screen keyboard
    if (hints & WEBKIT_INPUT_HINT_INHIBIT_OSK)
        return;

    // enable the key filter
    priv->keyModifierFilter = wpe_input_keyboard_modifier_virtual_keyboard;

    // emit a signal indicating we want the virtual keyboard to display
    g_signal_emit_by_name(context, "show-keyboard", nullptr);
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \overload

    Called when the user has removed focus from an HTML input element.

 */
static void virtualKeyboardInputMethodContextNotifyFocusOut(WebKitInputMethodContext* context)
{
    auto *self = reinterpret_cast<VirtualKeyboardInputMethodContext*>(context);
    auto *priv = reinterpret_cast<VirtualKeyboardInputMethodContextPrivate*>(
            virtual_keyboard_input_method_context_get_instance_private(self));

    //qInfo("virtualKeyboardInputMethodContextNotifyFocusOut");

    // remove the key filter
    priv->keyModifierFilter = 0;

    // if shown then emit a hide signal
    g_signal_emit_by_name(context, "hide-keyboard", nullptr);
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \overload

    Called when the cursor position has changed in the text field on the HTML
    page.

 */
static void virtualKeyboardInputMethodContextNotifySurrounding(WebKitInputMethodContext *context,
                                                               const gchar *text,
                                                               guint length,
                                                               guint cursor_index,
                                                               guint selection_index)
{
    auto *self = reinterpret_cast<VirtualKeyboardInputMethodContext*>(context);
    auto *priv = reinterpret_cast<VirtualKeyboardInputMethodContextPrivate*>(
            virtual_keyboard_input_method_context_get_instance_private(self));

    /*qInfo("virtualKeyboardInputMethodContextNotifySurrounding : text = '%s'",
          text); */

    // TODO: we can use the text argument here to set the initial text for the
    // virtual keyboard.
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \overload

    Called when a key input is received, this is where we can filter key events
    from propagating to the browser.

    This version is used by wpe 2.38 and older
 */
static gboolean virtualKeyboardInputMethodContextFilterKeyEventV1(WebKitInputMethodContext *context,
                                                                  struct wpe_input_keyboard_event *keyEvent)
{
    auto *self = reinterpret_cast<VirtualKeyboardInputMethodContext*>(context);
    auto *priv = reinterpret_cast<VirtualKeyboardInputMethodContextPrivate*>(
            virtual_keyboard_input_method_context_get_instance_private(self));

    /*qDebug("filtering key { code=%u, hw=%u, pressed=%s modifiers=0x%04x }",
           keyEvent->key_code, keyEvent->hardware_key_code,
           keyEvent->pressed ? "true" : "false", keyEvent->modifiers);
     */

    // check if filtering the key events based on their modifier value
    if (priv->keyModifierFilter && !(keyEvent->modifiers & priv->keyModifierFilter))
    {
        /*
        qDebug("blocking key code %u as modifier filter doesn't match "
               "(modifier: 0x%06x, filter: 0x%06x)",
               keyEvent->key_code, keyEvent->modifiers,
               priv->keyModifierFilter);
         */
        return TRUE;
    }

    return FALSE;
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \overload

    Called when a key input is received, this is where we can filter key events
    from propagating to the browser.

    This version is used by wpe 2.46 and onward
 */
static int virtualKeyboardInputMethodContextFilterKeyEventV2(WebKitInputMethodContext *context,
                                                             void *keyEvent)
{
    return (int) virtualKeyboardInputMethodContextFilterKeyEventV1(context, (struct wpe_input_keyboard_event *) keyEvent);
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \overload

    Sets the properties on the object, primarily used to set the key filter.

 */
static void virtualKeyboardInputMethodContextSetProperty(GObject *object,
                                                         guint propertyId,
                                                         const GValue *value,
                                                         GParamSpec *pspec)
{
    VirtualKeyboardInputMethodContext *self = VK_INPUT_METHOD_CONTEXT(object);
    g_return_if_fail(VK_IS_INPUT_METHOD_CONTEXT(object));

    auto *priv = reinterpret_cast<VirtualKeyboardInputMethodContextPrivate*>(
            virtual_keyboard_input_method_context_get_instance_private(self));

    switch (propertyId)
    {
        case PROP_FILTER_KEY_EVENTS:
            priv->keyModifierFilter = g_value_get_uint(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
            break;
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \overload

    Gets the properties on the object, primarily used to get the key filter.

 */
static void virtualKeyboardInputMethodContextGetProperty(GObject *object,
                                                         guint propertyId,
                                                         GValue *value,
                                                         GParamSpec *pspec)
{
    VirtualKeyboardInputMethodContext *self = VK_INPUT_METHOD_CONTEXT(object);
    g_return_if_fail(VK_IS_INPUT_METHOD_CONTEXT(object));

    auto *priv = reinterpret_cast<VirtualKeyboardInputMethodContextPrivate*>(
            virtual_keyboard_input_method_context_get_instance_private(self));

    switch (propertyId)
    {
        case PROP_FILTER_KEY_EVENTS:
            g_value_set_uint(value, priv->keyModifierFilter);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
            break;
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
void virtual_keyboard_input_method_context_class_init(VirtualKeyboardInputMethodContextClass *klass)
{
    //qInfo("initialising input context class");

    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->finalize = virtualKeyboardInputMethodContextFinalize;
    objectClass->set_property = virtualKeyboardInputMethodContextSetProperty;
    objectClass->get_property = virtualKeyboardInputMethodContextGetProperty;

    auto* imClass = WEBKIT_INPUT_METHOD_CONTEXT_CLASS(klass);
    imClass->notify_focus_in = virtualKeyboardInputMethodContextNotifyFocusIn;
    imClass->notify_focus_out = virtualKeyboardInputMethodContextNotifyFocusOut;
    imClass->notify_surrounding = virtualKeyboardInputMethodContextNotifySurrounding;

    /*if (WpeWebKitUtils::webkitVersion() < QVersionNumber(2, 46, 0))
    {
        imClass->filter_key_event =
                reinterpret_cast<typeof WebKitInputMethodContextClass::filter_key_event > (
                        virtualKeyboardInputMethodContextFilterKeyEventV1);
    }
    else
    {*/
        imClass->filter_key_event =
                reinterpret_cast<typeof WebKitInputMethodContextClass::filter_key_event > (
                        virtualKeyboardInputMethodContextFilterKeyEventV2);
   //}

    signals[SIGNAL_SHOW_KEYBOARD] = g_signal_new(
            "show-keyboard",
            G_TYPE_FROM_CLASS(klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(VirtualKeyboardInputMethodContextClass, show_keyboard),
            nullptr, nullptr,
            g_cclosure_marshal_generic,
            G_TYPE_NONE, 0);

    signals[SIGNAL_HIDE_KEYBOARD] = g_signal_new(
            "hide-keyboard",
            G_TYPE_FROM_CLASS(klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(VirtualKeyboardInputMethodContextClass, hide_keyboard),
            nullptr, nullptr,
            g_cclosure_marshal_generic,
            G_TYPE_NONE, 0);

    properties[PROP_FILTER_KEY_EVENTS] = g_param_spec_uint(
            "key-events-modifier-filter",
            "Filter key events prop",
            "Enables filtering on a key event's modifier field",
            0, UINT_MAX, 0,
            static_cast<GParamFlags>(G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

    g_object_class_install_properties(objectClass,
                                      N_PROPERTIES,
                                      properties);
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
void virtual_keyboard_input_method_context_init(VirtualKeyboardInputMethodContext *context)
{
   // qInfo("initialising input context object");

    auto *priv = reinterpret_cast<VirtualKeyboardInputMethodContextPrivate*>(
                virtual_keyboard_input_method_context_get_instance_private(context));

    priv->keyModifierFilter = 0;
}
