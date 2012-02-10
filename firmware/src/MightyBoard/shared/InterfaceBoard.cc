#include "InterfaceBoard.hh"
#include "Configuration.hh"
#include "LiquidCrystalSerial.hh"
#include "Host.hh"
#include "Timeout.hh"
#include "Command.hh"
#include "Diagnostics.hh"

#if defined HAS_INTERFACE_BOARD

Timeout button_timeout;
bool pop2 = false;

InterfaceBoard::InterfaceBoard(ButtonArray& buttons_in,
                               LiquidCrystalSerial& lcd_in,
                               const Pin& gled_in,
                               const Pin& rled_in,
                               Screen* mainScreen_in,
                               Screen* buildScreen_in,
                               MessageScreen* messageScreen_in) :
        lcd(lcd_in),
        buttons(buttons_in),
		waitingMask(0)
{
        buildScreen = buildScreen_in;
        mainScreen = mainScreen_in;
        messageScreen = messageScreen_in;
        LEDs[0] = gled_in;
        LEDs[1] = rled_in;
        buildPercentage = 101;
}

void InterfaceBoard::init() {
        buttons.init();

        lcd.begin(LCD_SCREEN_WIDTH, LCD_SCREEN_HEIGHT);
        lcd.clear();
        lcd.home();

	LEDs[0].setDirection(true);
	LEDs[1].setDirection(true);
	
    building = false;

    screenIndex = -1;
	waitingMask = 0;
#ifdef HEAT_DIAGNOSTICS
    pushScreen(&diagnostics);
#else
    pushScreen(mainScreen);
#endif
    
}

void InterfaceBoard::doInterrupt() {
	buttons.scanButtons();
}

micros_t InterfaceBoard::getUpdateRate() {
	return screenStack[screenIndex]->getUpdateRate();
}

/// push Error Message Screen
void InterfaceBoard::errorMessage(char buf[]){

		messageScreen->clearMessage();
		messageScreen->setXY(1,0);
		messageScreen->addMessage(buf, true);
		pushScreen(messageScreen);
}

void InterfaceBoard::doUpdate() {

	// If we are building, make sure we show a build menu; otherwise,
	// turn it off.
	switch(host::getHostState()) {
   //case host::HOST_STATE_ONBOARD_MONITOR:
    case host::HOST_STATE_BUILDING_ONBOARD:
            pop2 = true;
	case host::HOST_STATE_BUILDING:
	case host::HOST_STATE_BUILDING_FROM_SD:
		if (!building ){
			
			// if a message screen is still active, wait until it times out to push the monitor mode screen
			// move the current screen up an index so when it pops off, it will load buildScreen
			// as desired instead of popping to main menu first
			if(screenStack[screenIndex]->screenWaiting() || command::isWaiting())
			{
					if (screenIndex < SCREEN_STACK_DEPTH - 1) {
						screenIndex++;
						screenStack[screenIndex] = screenStack[screenIndex-1];
					}
					screenStack[screenIndex -1] = buildScreen;
					buildScreen->reset();
			}
			else
                 pushScreen(buildScreen);
			building = true;
		}
		break;
	default:
		if (building) {
			if(!(screenStack[screenIndex]->screenWaiting())){	
            //    if(pop2)
             //       pop2Screens();
              //  else
                    popScreen();
				building = false;
				if((screenStack[screenIndex] == buildScreen) && pop2){
					popScreen();
					pop2 = false;
				}
				///LEDTODO: set LEDs to white until button press
			}
		}
	
		break;
	}
    static ButtonArray::ButtonName button;

	if (buttons.getButton(button)) {
		if (button == ButtonArray::RESET){
			host::stopBuild();
			return;
		} else if (waitingMask != 0) {
			if (((1<<button) & waitingMask) != 0) {
				waitingMask = 0;
			}
		} else if (button == ButtonArray::EGG){
			pushScreen(&snake);
		} else {
			screenStack[screenIndex]->notifyButtonPressed(button);
			if(screenStack[screenIndex]->continuousButtons()) {
				button_timeout.start(ButtonArray::ButtonDelay);// 1s timeout 
			}
		}
	}
	// clear button press if button timeout occurs in continuous press mode
	if(button_timeout.hasElapsed())
	{
		buttons.clearButtonPress();
		button_timeout.clear();
	}

	screenStack[screenIndex]->setBuildPercentage(buildPercentage);	
	screenStack[screenIndex]->update(lcd, false);
}

void InterfaceBoard::pushScreen(Screen* newScreen) {
	if (screenIndex < SCREEN_STACK_DEPTH - 1) {
		screenIndex++;
		screenStack[screenIndex] = newScreen;
	}
	screenStack[screenIndex]->reset();
	screenStack[screenIndex]->update(lcd, true);
}
void InterfaceBoard::setBuildPercentage(uint8_t percent){
	buildPercentage = percent;
}

void InterfaceBoard::popScreen() {
	// Don't allow the root menu to be removed.
	if (screenIndex > 0) {
		screenIndex--;
	}

	screenStack[screenIndex]->update(lcd, true);
}
void InterfaceBoard::pop2Screens() {
	// Don't allow the root menu to be removed.
	if (screenIndex > 1) {
		screenIndex-=2;
	}
    
	screenStack[screenIndex]->update(lcd, true);
}

void InterfaceBoard::setLED(uint8_t id, bool on){
	LEDs[id].setValue(on);
}


/// Tell the interface board that the system is waiting for a button push
/// corresponding to one of the bits in the button mask. The interface board
/// will not process button pushes directly until one of the buttons in the
/// mask is pushed.
void InterfaceBoard::waitForButton(uint8_t button_mask) {
  waitingMask = button_mask;
}
void InterfaceBoard::clearButtonWait(){
	waitingMask = 0;
}

/// Check if the expected button push has been made. If waitForButton was
/// never called, always return true.
bool InterfaceBoard::buttonPushed() {
  return waitingMask == 0;
}

#endif
