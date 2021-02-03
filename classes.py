# from __future__ import absolute_import, division, print_function
from usb_1608FS import *
# from time import sleep
# from ctypes import cast, POINTER, c_double, c_ushort, c_ulong
from tkinter import *


class DataLogger():
    # MCC DAQ USB1608FS
    # https://github.com/mccdaq/mcculw
    def __init__(self, daq_device):
        #   # initalize the class
        # try:
        #     self.usb1608FS = usb_1608FS()
        # except:
        #     print('No USB-1608FS device found.') 
        
        self.daq_device = daq_device

        # set gains
        self.gains = [0]*8
        self.gains[0] = self.daq_device.BP_10_00V
        self.gains[1] = self.daq_device.BP_10_00V

        self.num_points = 0
        self.rate = 4000

        #set options to external sync, rising edge
        self.options = self.daq_device.AIN_EXTERN_SYNC

        # set sync type to gated clock, so it is slaved to the trigger in from the arduino.
        # when the rising edge of the pulse comes in, it will collect data
        self.daq_device.SetSync(2)      
        
    def device_info(self):
    #   print("Manufacturer: %s" % self.daq_device.getManufacturer())
      product = self.daq_device.getProduct()
      serial_number = self.daq_device.getSerialNumber()
      return product, serial_number



    def ready_analog_scan(self):

        # get the analog input info
        # ai_info = daq_dev_info.get_ai_info()
        
        # define the channels
        low_chan = 0
        high_chan = 1
        
        # acquire data
        data = self.daq_device.AInScan(low_chan, high_chan, self.gains, self.num_points, self.rate, self.options)

        self.console_out(data, self.num_points, self.gains[0])

        self.daq_device.AInStop()


    def reset_daq(self):
        self.daq_device.AInStop()
        response = "DAQ reset."
        return response

    def console_out(self, data, count, gain):
        for i in range(count):
            print('data[',i,'] = ', hex(data[i]), '\t', format(self.daq_device.volts(gain, data[i]), '.3f'), 'V')
    

class TraceController():
    def __init__(self, arduino):
        try:
            self.arduino = arduino
        except:
            print("No Arduino found on port ttyUSB0")

    def get_diagnostic_info(self):
        data = self.arduino
        return data

    def set_parameters(self, cmd_input):
        ser = self.arduino
        ser.timeout = 1
        ser.write(cmd_input)
        response = ser.readlines()
        return response



# class SerialListener(threading.Thread):
#     def __init__(self, arduino):
#         super().__init__()
#         self._stop_event = threading.Event()
#         try:
#             self.arduino = arduino
#         except:
#             print("No Arduino found on port ttyUSB0")

#     def run(self):
#         logger.debug("Serial listener started")
#         previous = 0
#         while not self._stop_event.is_set():


        




# class Window(Frame):
#     def __init__(self, master=None, datalogger=None, tracecontrol=None):
#         Frame.__init__(self, master)        
#         self.master = master
#         datalogger = datalogger
#         tracecontrol = tracecontrol

#         # widget can take all window
#         # self.pack(fill=BOTH, expand=1)
    
#         # create button, link it to clickExitButton()
#         exitButton = Button(self, text="Exit", command=self.clickExitButton)
#         deviceInfoButton = Button(self.master, text="DAQ Device Info",command=datalogger.device_info)
#         arduinoPortButton = Button(self.master, text="Arduino Port Location", command=tracecontrol.get_diagnostic_info)

#         # put it on the screen
#         deviceInfoButton.grid(row=1, column=1)
#         arduinoPortButton.grid(row=2, column=1)
#         exitButton.place(row=3, column=1)

#     def clickExitButton(self):
#         exit()


# class TextHandler(logging.Handler):
#     # this calss allows you to log to a tkinter text or scrolledtext widget
#     def __init__(self, text):
#         logging.Handler.__init__(self)
#         # store a reference to the text it will log to
#         self.text = text

#     def emit(self, record):
#         msg = self.format(record)
#         def append():
#             self.text.configure(state='normal')
#             self.text.insert(Tkinter.END, msg + '\n')
#             self.text.configure(state='disabled')
#             self.text.yview(Tkinter.END)
#         self.text.after(0, append)

