import datetime
import queue
import logging
import signal
import time
import threading
import tkinter as tk
from tkinter.scrolledtext import ScrolledText
from tkinter import ttk, VERTICAL, HORIZONTAL, N, S, E, W
from usb_1608FS import *
from classes import TraceController, DataLogger
import serial


# initalize the daq
# initialize the usb daq
try:
  usb1608FS = usb_1608FS()  
except:
  print('No USB-1608FS device found.')

# initialize the serial connection to arduino
try:
  ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
except:
  print("could not initialize tracecontroller")

# initialize tracecontroller and pass it the serial connection
tracecontrol = TraceController(ser)

# initialize the datalogger and pass it the daq
datalogger = DataLogger(usb1608FS)

# start the logger
logger = logging.getLogger(__name__)

# class Clock(threading.Thread):
#     """Class to display the time every seconds

#     Every 5 seconds, the time is displayed using the logging.ERROR level
#     to show that different colors are associated to the log levels
#     """

#     def __init__(self):
#         super().__init__()
#         self._stop_event = threading.Event()

#     def run(self):
#         logger.debug('Clock started')
#         previous = -1
#         while not self._stop_event.is_set():
#             now = datetime.datetime.now()
#             if previous != now.second:
#                 previous = now.second
#                 if now.second % 5 == 0:
#                     level = logging.ERROR
#                 else:
#                     level = logging.INFO
#                 logger.log(level, now)
#             time.sleep(0.2)

#     def stop(self):
#         self._stop_event.set()


class QueueHandler(logging.Handler):
    """Class to send logging records to a queue

    It can be used from different threads
    The ConsoleUi class polls this queue to display records in a ScrolledText widget
    """
    # Example from Moshe Kaplan: https://gist.github.com/moshekaplan/c425f861de7bbf28ef06
    # (https://stackoverflow.com/questions/13318742/python-logging-to-tkinter-text-widget) is not thread safe!
    # See https://stackoverflow.com/questions/43909849/tkinter-python-crashes-on-new-thread-trying-to-log-on-main-thread

    def __init__(self, log_queue):
        super().__init__()
        self.log_queue = log_queue

    def emit(self, record):
        self.log_queue.put(record)


class ConsoleUi:
    """Poll messages from a logging queue and display them in a scrolled text widget"""

    def __init__(self, frame):
        self.frame = frame
        # Create a ScrolledText wdiget
        self.scrolled_text = ScrolledText(frame, state='disabled', height=12)
        self.scrolled_text.grid(row=0, column=0, sticky=(N, S, W, E))
        self.scrolled_text.configure(font='TkFixedFont')
        self.scrolled_text.tag_config('INFO', foreground='black')
        self.scrolled_text.tag_config('DEBUG', foreground='gray')
        self.scrolled_text.tag_config('WARNING', foreground='orange')
        self.scrolled_text.tag_config('ERROR', foreground='red')
        self.scrolled_text.tag_config('CRITICAL', foreground='red', underline=1)
        # Create a logging handler using a queue
        self.log_queue = queue.Queue()
        self.queue_handler = QueueHandler(self.log_queue)
        formatter = logging.Formatter('%(asctime)s: %(message)s')
        self.queue_handler.setFormatter(formatter)
        logger.addHandler(self.queue_handler)
        # Start polling messages from the queue
        self.frame.after(100, self.poll_log_queue)

    def display(self, record):
        msg = self.queue_handler.format(record)
        self.scrolled_text.configure(state='normal')
        self.scrolled_text.insert(tk.END, msg + '\n', record.levelname)
        self.scrolled_text.configure(state='disabled')
        # Autoscroll to the bottom
        self.scrolled_text.yview(tk.END)

    def poll_log_queue(self):
        # Check every 100ms if there is a new message in the queue to display
        while True:
            try:
                record = self.log_queue.get(block=False)
            except queue.Empty:
                break
            else:
                self.display(record)
        self.frame.after(100, self.poll_log_queue)


class FormUi:

    def __init__(self, frame):
        self.frame = frame
        # Create a combobbox to select the logging level
        # values = ['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL']
        # self.level = 'DEBUG'
        self.level = tk.StringVar()
        # ttk.Label(self.frame, text='Level:').grid(column=0, row=0, sticky=W)
        # self.combobox = ttk.Combobox(
        #     self.frame,
        #     textvariable=self.level,
        #     width=25,
        #     state='readonly',
        #     values=values
        # )
        # self.combobox.current(0)
        # self.combobox.grid(column=1, row=0, sticky=(W, E))
       
        # Create a text field to enter a message
        self.ard_cmd = tk.StringVar()
        self.ard_cmd.set("n2000 i248 g0 h0 v1 r5 p40 m")
        self.txt_arduino_cmd = ttk.Entry(self.frame, textvariable=self.ard_cmd, width=25)

        # labels
        # self.lbl_arduino_input = ttk.Label(self.frame, text='tracecon_command:')

        # Add buttons
        self.btn_send_to_arduino = ttk.Button(self.frame, width=25, text='Serial to Arduino >', command=self.process_ard_cmd)
        self.btn_get_daq_info = ttk.Button(self.frame, width=25, text="Get DAQ info", command=self.get_daq_info)
        self.btn_get_arduino_info = ttk.Button(self.frame, width=25, text="Get Arduino info", command=self.get_diagnostic_info)
        self.btn_reset_daq = ttk.Button(self.frame, width=25, text="reset DAQ", command=self.reset_daq)
        self.btn_ready_daq = ttk.Button(self.frame, width=25, text="ready DAQ", command=self.ready_daq)
        
        # arrange buttons and gui components
        # self.lbl_arduino_input.grid(column=0, row=5, sticky=W)
        self.txt_arduino_cmd.grid(column=1, row=5, sticky=(W, E))
        self.btn_send_to_arduino.grid(column=0, row=5, sticky=W)
        
        self.btn_get_daq_info.grid(column=0, row=1, sticky=W)
        self.btn_ready_daq.grid(column=1, row=1)
        self.btn_reset_daq.grid(column=2, row=1)

        self.btn_get_arduino_info.grid(column=0, row=2)
        

    def process_ard_cmd(self):
        bin_input = bytearray(self.ard_cmd.get(), "utf8")
        logger.log(logging.INFO, bin_input)
        ard_response = tracecontrol.set_parameters(bin_input)
        logger.log(logging.INFO, ard_response)

    # functions for buttons
    def get_diagnostic_info(self):
        diag_info = tracecontrol.get_diagnostic_info()
        logger.log(logging.INFO, diag_info)

    def get_daq_info(self):
        product, serial_number = datalogger.device_info()
        log_msg = "Prod: " + product + ", SN: " + serial_number
        logger.log(logging.INFO, log_msg)

    def ready_daq(self):
         datalogger.ready_analog_scan()
         logger.log(logging.INFO, "DAQ waiting for trigger.")

    def reset_daq(self):
        daq_response = datalogger.reset_daq()
        logger.log(logging.INFO, daq_response)


# class ThirdUi:

#     def __init__(self, frame):
#         self.frame = frame
#         ttk.Label(self.frame, text='This is just an example of a third frame').grid(column=0, row=1, sticky=W)
#         ttk.Label(self.frame, text='With another line here!').grid(column=0, row=4, sticky=W)


class App:

    def __init__(self, root):
        self.root = root
        root.title('Experiment Control GUI')
        root.columnconfigure(0, weight=1)
        root.rowconfigure(0, weight=1)
        # Create the panes and frames
        vertical_pane = ttk.PanedWindow(self.root, orient=VERTICAL)
        vertical_pane.grid(row=0, column=0, sticky="nsew")
        horizontal_pane = ttk.PanedWindow(vertical_pane, orient=HORIZONTAL)
        vertical_pane.add(horizontal_pane)
        form_frame = ttk.Labelframe(horizontal_pane, text="Controls")
        form_frame.columnconfigure(1, weight=1)
        horizontal_pane.add(form_frame, weight=1)
        console_frame = ttk.Labelframe(horizontal_pane, text="Console")
        console_frame.columnconfigure(0, weight=1)
        console_frame.rowconfigure(0, weight=1)
        horizontal_pane.add(console_frame, weight=1)
        # third_frame = ttk.Labelframe(vertical_pane, text="Third Frame")
        # vertical_pane.add(third_frame, weight=1)
        # Initialize all frames
        self.form = FormUi(form_frame)
        self.console = ConsoleUi(console_frame)
        # self.third = ThirdUi(third_frame)
        # self.clock = Clock()
        # self.clock.start()
        self.root.protocol('WM_DELETE_WINDOW', self.quit)
        self.root.bind('<Control-q>', self.quit)
        signal.signal(signal.SIGINT, self.quit)
# Frame
    def quit(self, *args):
        # self.clock.stop()
        self.root.destroy()


def main():
    logging.basicConfig(level=logging.DEBUG)
    root = tk.Tk()
    app = App(root)
    app.root.mainloop()


if __name__ == '__main__':
    main()
