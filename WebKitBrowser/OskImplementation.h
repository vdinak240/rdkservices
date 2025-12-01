#ifndef OSKIMPLEMENTATION_H
#define OSKIMPLEMENTATION_H

#include <glib.h>
#include <gio/gio.h>
#include <gio-unix-2.0/gio/gunixinputstream.h>
#include <signal.h>
#include <sys/prctl.h>
#include <cstdint>
#include <vector>
#include <string>
#include <mutex>
#include "VirtualKeyboardInputMethod.h"

namespace WPEFramework {
namespace Plugin {
class OskImplementation
{
public: 
    
    void Initialize(WebKitWebView *view,std::string language,bool isVGenabled)
    {   
        
        m_view = view;
        m_language = language;
        m_isVGenabled = isVGenabled;

        VirtualKeyboardInputMethodContext *inputContext =
               VK_INPUT_METHOD_CONTEXT(g_object_new(VK_TYPE_INPUT_METHOD_CONTEXT,
                                             nullptr));

        if (!inputContext)
        {
            SYSLOG(Logging::Notification, (_T("Failed to get the Virtual Keyboard input context")));
            return;
        }
        
        
        m_inputContext = std::unique_ptr<VirtualKeyboardInputMethodContext, decltype(&g_object_unref)>(
                              inputContext, &g_object_unref);

        webkit_web_view_set_input_method_context(m_view, WEBKIT_INPUT_METHOD_CONTEXT(m_inputContext.get()));
        //Q_ASSERT(webkit_web_view_get_input_method_context(_view) == WEBKIT_INPUT_METHOD_CONTEXT(m_inputContext.get()));

        // connect to the signals notifying us to show / hide the virtual keyboard
        g_signal_connect(m_inputContext.get(), "show-keyboard",
                 G_CALLBACK(OskImplementation::showVirtualKeyboardCallback), this);
        g_signal_connect(m_inputContext.get(), "hide-keyboard",
                 G_CALLBACK(OskImplementation::hideVirtualKeyboardCallback), this);
    }
    wpe_input_keyboard_event createWpeKeyboardEvent(uint32_t wpeKeyCode,
                                                               bool pressed)
    {
        struct wpe_input_xkb_keymap_entry* entries = nullptr;
        uint32_t entriesCount = 0;

        wpe_input_xkb_context_get_entries_for_key_code(wpe_input_xkb_context_get_default(),
                                                   wpeKeyCode,
                                                   &entries, &entriesCount);

        wpe_input_keyboard_event event = {
            0,
            wpeKeyCode,
            entriesCount ? entries[0].hardware_key_code : 0,
            pressed,
            wpe_input_keyboard_modifier_virtual_keyboard
        };

        free(entries);

        return event;

    }

    bool injectKeyEvent( uint16_t keyCode)
    {
        auto* backend = webkit_web_view_backend_get_wpe_backend(webkit_web_view_get_backend(m_view));
        if (!backend)
        {
            SYSLOG(Logging::Notification, (_T("Failed to get WPE backend - cannot inject key events")));
            return false;
        }

        // Convert Unicode to WPE key code
        uint32_t wpeKeyCode = wpe_unicode_to_key_code(keyCode);

        // Create and dispatch WPE keyboard event
        wpe_input_keyboard_event wpeEvent = createWpeKeyboardEvent(wpeKeyCode, true);
        wpe_view_backend_dispatch_keyboard_event(backend, &wpeEvent);
        return true;
    }

    void destroyOsk()
    {    
        std::lock_guard<std::mutex> lock(process_mutex);
        if (oskprocess) {
            g_subprocess_force_exit(oskprocess);
            g_clear_object(&oskprocess);
            SYSLOG(Logging::Notification, (_T("Osk process killed ")));
        }
        if (m_source) {
            g_source_destroy(m_source);
            m_source = nullptr;
        }
    }
    void launchOsK(const std::vector<std::string>& args)
    {
        if (args.empty()) {
            SYSLOG(Logging::Notification, (_T("No arguments provided for spawning process")));
            return;
        }

        GError *error = nullptr;

        // Prepare argv: Convert vector to null-terminated gchar** array
        std::vector<gchar*> argv;
        for (const auto& arg : args) {
            argv.push_back(g_strdup(arg.c_str()));  // Duplicate strings for GLib
        }
        argv.push_back(nullptr);  // Null-terminate

        GSubprocessLauncher *launcher = g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE);
        g_subprocess_launcher_set_child_setup(launcher, [](gpointer) {
            prctl(PR_SET_PDEATHSIG, SIGKILL);
        }, nullptr, nullptr);

       {
           std::lock_guard<std::mutex> lock(process_mutex);
           oskprocess = g_subprocess_launcher_spawnv(launcher, argv.data(), &error);
       }

       // Free the duplicated strings
       for (auto& ptr : argv) {
           if (ptr) g_free(ptr);
       }
       if (!oskprocess) {
           g_clear_error(&error);
           g_object_unref(launcher);
           return;
       }

        std::cout << "Osk process PID: " << g_subprocess_get_identifier(oskprocess) << std::endl;
        GInputStream *stdout_stream = g_subprocess_get_stdout_pipe(oskprocess);
        int fd = g_unix_input_stream_get_fd(G_UNIX_INPUT_STREAM(stdout_stream));
        if (fd == -1) {
            std::cout << "Failed to get file descriptor." << std::endl;
            g_object_unref(stdout_stream);
            g_object_unref(oskprocess);
            g_object_unref(launcher);
            return;
        }

        GIOChannel *channel = g_io_channel_unix_new(fd);
        g_io_channel_set_encoding(channel, nullptr, nullptr);
        m_source = g_io_create_watch(channel, static_cast<GIOCondition>( G_IO_IN | G_IO_HUP ));
        g_source_set_callback(m_source,
                          (GSourceFunc) readDataTrampoline,
                          this, nullptr);
        g_source_attach(m_source, g_main_context_get_thread_default());
        g_io_channel_unref(channel);
    }

    
    static gboolean readDataTrampoline(GIOChannel* source, GIOCondition condition, gpointer data)
    {
        auto* self = static_cast<OskImplementation*>(data);
        return self->readData(source, condition, data);
    }

    gboolean readData(GIOChannel *source, GIOCondition condition, gpointer data) {
        static int debug_counter = 0;
        fprintf(stderr, "Callback triggered #%d, condition: %d\n", ++debug_counter, condition);
        if (condition & G_IO_IN) {
            gchar buf[2];
            gsize bytes_read;
            GError *error = nullptr;

            GIOStatus status = g_io_channel_read_chars(source, buf, sizeof(buf), &bytes_read, &error);
            if (status == G_IO_STATUS_NORMAL && bytes_read == 2) {
                uint16_t utf16_code = (static_cast<uint8_t>(buf[1]) << 8) | static_cast<uint8_t>(buf[0]);
               // Check for backspace (U+0008) or enter (U+000D)
                if (utf16_code == 0x0008 || utf16_code == 0x000D) {
                    fprintf(stderr, "Received special character (U+%04X), killing Osk process.\n", utf16_code);
                    destroyOsk();  // Kill the child instead of printing
                }
                // Convert UTF-16 (little-endian, no BOM assumed) to UTF-8
                else
                {
                    //send key to browser
                    if(injectKeyEvent(utf16_code))
                     {
                         std::cout << "key event send to browser" << std::endl;
                     }
                }
                
            }
            else if (bytes_read != 2) {
            // Partial read: Log but don't process (wait for full char)
                fprintf(stderr, "Partial read: %zu bytes (expected 2)\n", bytes_read);
            }
            if (error) {
                std::cerr << "Error reading stdout: " << error->message << std::endl;
                g_clear_error(&error);
            }
        }
        if (condition & G_IO_HUP) {
            std::cout << "Stdout pipe closed (HUP)." << std::endl;
            destroyOsk();
            return FALSE;  // Stop watching on hangup
        }
        return TRUE;  // Keep callback active    
    }

    
   static void showVirtualKeyboardCallback(WebKitInputMethodContext *inputContext, void *userData) {
        auto *self = static_cast<OskImplementation*>(userData);
        self->showVirtualKeyboard(inputContext);
    }

    static void hideVirtualKeyboardCallback(WebKitInputMethodContext *inputContext, void *userData) {
        auto *self = static_cast<OskImplementation*>(userData);
        self->hideVirtualKeyboard(inputContext);
    }

private:


   void showVirtualKeyboard(WebKitInputMethodContext *inputContext)
   {
       //auto* browser = static_cast<WebKitImplementation*>(userData);
                // get the input details
       std::string type;
       const WebKitInputPurpose purpose = webkit_input_method_context_get_input_purpose(inputContext);
       //const WebKitInputHints hints = webkit_input_method_context_get_input_hints(inputContext);
       
       switch (purpose) {
           case WEBKIT_INPUT_PURPOSE_EMAIL: type = "email"; break;
           case WEBKIT_INPUT_PURPOSE_DIGITS: type = "numeric"; break;
           default: type = "text"; break;
       }

      //add command line argument and start
       std::vector<std::string> args = {
                "/usr/bin/osk",  // Executable path
                "--type", type,
                "--position", "top",
                "--language", m_language ,
                "--font", "<path to font>"
       };

       if (m_isVGenabled) {
           args.push_back("--voiceguidance");
       }
       
       {
            std::lock_guard<std::mutex> lock(process_mutex);
            if (oskprocess != nullptr && g_subprocess_get_identifier(oskprocess) != nullptr) {
                std::cout << "OSK is already running. Skipping launch." << std::endl;
                return;
            }
       }   

       // launch osk
       launchOsK(args);

   }
   void hideVirtualKeyboard(WebKitInputMethodContext *context)
   {
       destroyOsk();
       oskprocess = nullptr;
   }

   WebKitWebView* m_view;
   std::string m_language;
   GSource *m_source;
   std::unique_ptr<VirtualKeyboardInputMethodContext, decltype(&g_object_unref)> m_inputContext{nullptr,&g_object_unref};
   bool m_isVGenabled;
   GSubprocess *oskprocess = nullptr;
   std::mutex process_mutex;
};

}//namespace Plugin
} //namespace WPEFramework

#endif
