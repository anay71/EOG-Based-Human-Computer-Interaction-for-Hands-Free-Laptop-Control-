import serial
import serial.tools.list_ports
import pyautogui
import time
from datetime import datetime

# =========================================================
# CONFIGURATION
# =========================================================

BAUD_RATE = 115200

# Cursor movement
CURSOR_STEP = 40
CURSOR_DURATION = 0.05

# Scroll amount
SCROLL_STEP = 120

# Delay between repeated actions
ACTION_DELAY = 0.02

# =========================================================
# ACTION MAP
# =========================================================
# Cursor movement + scrolling + clicking all supported

ACTION_MAP = {

    # -------------------------
    # Blink Actions
    # -------------------------
    "BLINK:SINGLE": {
        "type": "click",
        "button": "left"
    },

    "BLINK:DOUBLE": {
        "type": "click",
        "button": "right"
    },

    "BLINK:TRIPLE": {
        "type": "key",
        "value": "enter"
    },

    # -------------------------
    # Cursor Movement
    # -------------------------
    "MOVE:LEFT": {
        "type": "cursor",
        "x": -CURSOR_STEP,
        "y": 0
    },

    "MOVE:RIGHT": {
        "type": "cursor",
        "x": CURSOR_STEP,
        "y": 0
    },

    "MOVE:UP": {
        "type": "cursor",
        "x": 0,
        "y": -CURSOR_STEP
    },

    "MOVE:DOWN": {
        "type": "cursor",
        "x": 0,
        "y": CURSOR_STEP
    },

    # -------------------------
    # Scrolling
    # -------------------------
    "SCROLL:UP": {
        "type": "scroll",
        "amount": SCROLL_STEP
    },

    "SCROLL:DOWN": {
        "type": "scroll",
        "amount": -SCROLL_STEP
    },

    # Optional Horizontal Scroll
    "SCROLL:LEFT": {
        "type": "hscroll",
        "amount": -SCROLL_STEP
    },

    "SCROLL:RIGHT": {
        "type": "hscroll",
        "amount": SCROLL_STEP
    }
}


# =========================================================
# EOG CONTROLLER CLASS
# =========================================================

class EOGController:

    def __init__(self):

        self.ser = None

        # Safety
        pyautogui.FAILSAFE = True

        # Small internal pause
        pyautogui.PAUSE = 0.005

        # Prevent event flooding
        self.last_action_time = 0

    # =====================================================
    # Find ESP32 Port
    # =====================================================

    def find_port(self):

        ports = serial.tools.list_ports.comports()

        for p in ports:

            desc = str(p.description)

            if any(keyword in desc for keyword in [
                "CP210",
                "CH340",
                "USB",
                "ESP32",
                "Silicon Labs"
            ]):

                return p.device

        return None

    # =====================================================
    # Connect Serial
    # =====================================================

    def connect(self):

        port = self.find_port()

        if not port:
            print("ERROR: ESP32 not found.")
            return False

        try:

            self.ser = serial.Serial(
                port,
                BAUD_RATE,
                timeout=0.1
            )

            time.sleep(2)

            print(f"Connected to {port}")

            return True

        except Exception as e:

            print(f"Connection Error: {e}")

            return False

    # =====================================================
    # Execute Actions
    # =====================================================

    def execute_action(self, action):

        try:

            # ---------------------------------------------
            # Cursor Movement
            # ---------------------------------------------
            if action["type"] == "cursor":

                pyautogui.moveRel(
                    action["x"],
                    action["y"],
                    duration=CURSOR_DURATION
                )

            # ---------------------------------------------
            # Vertical Scroll
            # ---------------------------------------------
            elif action["type"] == "scroll":

                pyautogui.scroll(
                    action["amount"]
                )

            # ---------------------------------------------
            # Horizontal Scroll
            # ---------------------------------------------
            elif action["type"] == "hscroll":

                pyautogui.hscroll(
                    action["amount"]
                )

            # ---------------------------------------------
            # Mouse Click
            # ---------------------------------------------
            elif action["type"] == "click":

                pyautogui.click(
                    button=action["button"]
                )

            # ---------------------------------------------
            # Keyboard Press
            # ---------------------------------------------
            elif action["type"] == "key":

                pyautogui.press(
                    action["value"]
                )

        except Exception as e:

            print(f"Action Failed: {e}")

    # =====================================================
    # Main Loop
    # =====================================================

    def run(self):

        if not self.connect():
            return

        print("\n===================================")
        print("EOG CONTROLLER ACTIVE")
        print("===================================")

        print("\nControls:")
        print("Single Blink  -> Left Click")
        print("Double Blink  -> Right Click")
        print("Triple Blink  -> Enter Key")
        print("Eye Movement  -> Cursor Move")
        print("Scroll Events -> Page Scroll")

        print("\nPress CTRL+C to stop.\n")

        try:

            while True:

                if self.ser.in_waiting > 0:

                    line = self.ser.readline() \
                        .decode('utf-8', errors='ignore') \
                        .strip()

                    if not line:
                        continue

                    # Prevent too-fast repeated actions
                    current_time = time.time()

                    if current_time - self.last_action_time < ACTION_DELAY:
                        continue

                    self.last_action_time = current_time

                    # ---------------------------------
                    # Execute Actions
                    # ---------------------------------
                    if line in ACTION_MAP:

                        action = ACTION_MAP[line]

                        now = datetime.now() \
                            .strftime("%H:%M:%S.%f")[:-3]

                        print(f"[{now}] {line}")

                        self.execute_action(action)

                    # ---------------------------------
                    # Status Messages
                    # ---------------------------------
                    elif "STATUS" in line:

                        print(f"STATUS: {line}")

        except KeyboardInterrupt:

            print("\nStopping Controller...")

        finally:

            if self.ser:
                self.ser.close()

            print("Serial connection closed.")


# =========================================================
# MAIN
# =========================================================

if __name__ == "__main__":

    controller = EOGController()

    controller.run()