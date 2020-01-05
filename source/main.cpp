#include <string.h>
#include <stdio.h>
#include <chrono>

#include <switch.h>
#include <thread>

#include "gui.hpp"
#include "gui_main.hpp"
#include "gui_sysmodule.hpp"
#include "gui_hekate.hpp"
#include "gui_hid_mitm.hpp"
#include "gui_overrides_menu.hpp"
#include "gui_override_key.hpp"
#include "gui_title_list.hpp"
#include "button.hpp"

#include "threads.hpp"

extern "C" {
  #include "hid_extra.h"
}

static Gui *currGui = nullptr;
static bool updateThreadRunning = false;
static Mutex mutexCurrGui;
u32 __nx_applet_type = AppletType_Default;

bool g_exitApplet = false;

void update() {
  while (updateThreadRunning) {
    auto begin = std::chrono::steady_clock::now();

    mutexLock(&mutexCurrGui);
    if (currGui != nullptr)
      currGui->update();

    mutexUnlock(&mutexCurrGui);

    svcSleepThread(1.0E6 - std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - begin).count());
  }
}

int main(int argc, char **argv){
    u64 kdown = 0;
    touchPosition touch;
    u8 touchCntOld = 0, touchCnt = 0;

    socketInitializeDefault();
    nxlinkStdio();

    hidExtraInitialize();

    setsysInitialize();
    ColorSetId colorSetId;
    setsysGetColorSetId(&colorSetId);
    setTheme(colorSetId);
    setsysExit();
    nsInitialize();

    framebufferCreate(&Gui::g_fb_obj, nwindowGetDefault(), 1280, 720, PIXEL_FORMAT_RGBA_8888, 2);
    framebufferMakeLinear(&Gui::g_fb_obj);

    Gui::g_nextGui = GUI_MAIN;

    mutexInit(&mutexCurrGui);

    updateThreadRunning = true;
    std::thread updateThread(update);
    

    touchCntOld = hidTouchCount();

    while(appletMainLoop()) {
      hidScanInput();
      kdown = 0;
      for (u8 controller = 0; controller < 10; controller++) {
        kdown |= hidKeysDown(static_cast<HidControllerID>(controller));
      }

      if (Gui::g_nextGui != GUI_INVALID) {
        mutexLock(&mutexCurrGui);

        delete currGui;

        Button::pageOffsetX = 0;
        Button::pageOffsetY = 0;
        Button::targetOffsetX = 0;
        Button::targetOffsetY = 0;

        switch(Gui::g_nextGui) {
          case GUI_MAIN:
            currGui = new GuiMain();
            break;
          case GUI_SM_SELECT:
            currGui = new GuiSysmodule();
            break;
          case GUI_HEKATE:
            currGui = new GuiHekate();
            break;
          case GUI_HID_MITM:
            currGui = new GuiHIDMitm();
            break;
          case GUI_OVERRIDES_MENU:
            currGui = new GuiOverridesMenu();
            break;
          case GUI_OVERRIDE_KEY:
            currGui = new GuiOverrideKey();
            break;
          case GUI_TITLE_LIST:
            currGui = new GuiTitleList();
          default:
            break;
        }
        mutexUnlock(&mutexCurrGui);
        Gui::g_nextGui = GUI_INVALID;
      }

      if(currGui != nullptr) {
        Button::targetOffsetX = Lerp(Button::targetOffsetX, Button::pageOffsetX, SCROLL_SPEED);
        Button::targetOffsetY = Lerp(Button::targetOffsetY, Button::pageOffsetY, SCROLL_SPEED);
        currGui->draw();

        if (kdown) {
          if(Gui::g_currListSelector != nullptr)
            Gui::g_currListSelector->onInput(kdown);
          else if(Gui::g_currMessageBox != nullptr)
            Gui::g_currMessageBox->onInput(kdown);
          else currGui->onInput(kdown);
        }

        touchCnt = hidTouchCount();

        if (touchCnt > touchCntOld) {
          hidTouchRead(&touch, 0);
          if(Gui::g_currListSelector != nullptr)
            Gui::g_currListSelector->onTouch(touch);
          else currGui->onTouch(touch);
        }

        touchCntOld = touchCnt;

        if (g_exitApplet)
          break;

      }
    }

    if (currGui != nullptr)
      delete currGui;

    updateThreadRunning = false;
    updateThread.join();
    nsExit();
    socketExit();
    hidExtraExit();
    framebufferClose(&Gui::g_fb_obj);

    appletEndBlockingHomeButton();

    return 0;
}
