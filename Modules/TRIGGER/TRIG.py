import gpiod
import time


class TriggerNode:
    def __init__(self, but_pin=112, led_pin=144):
        print("[TRIGGER NODE]: initilizing GPIOs ...")
        self.chip = gpiod.Chip("gpiochip0")
        self.but = self.chip.get_line(but_pin)
        self.led = self.chip.get_line(led_pin)
        self.led_state = 0
        self.but.request(consumer = "Button", type = gpiod.LINE_REQ_EV_FALLING_EDGE)
        self.led.request(consumer = "LED", type = gpiod.LINE_REQ_DIR_OUT)
        self.led.set_value(0)

    def wait_for_press(self):
        if self.but.event_wait(sec=1):
            self.but.event_read()
            self.led_state = not self.led_state
            self.led.set_value(1 if self.led_state else 0)

            #clear garbage values
            while self.but.event_wait(sec=0):
                self.but.event_read()

            time.sleep(0.2)
            return True, self.led_state
        return False, self.led_state
    
    def cleanup(self):
        self.but.release()
        self.led.release()
        self.chip.close()
        print("[TRIGGER NODE]: GPIO RELEASED")
 

