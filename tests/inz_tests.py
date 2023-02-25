import pytest
import serial_com
import time
from firebase import firebase
import random
from datetime import datetime

def generate_automatic_temp():
    set_temp = random.uniform(-20,50)
    curr_temp = random.uniform(-20,50)
    expected_heater_status = True if set_temp > curr_temp else False
    return set_temp, curr_temp, expected_heater_status

def generate_schedule_time():
    start_hour = random.randrange(0,24)
    start_minute = random.randrange(0,59)
    stop_hour = random.randrange(0,24)
    stop_minute = random.randrange(0,59)
    status = random.randint(0,1)
    return start_hour, start_minute, stop_hour, stop_minute, status

def generate_schedule_temp():
    set_temp = random.uniform(10,40)
    current_temp = random.uniform(-10,10)
    return set_temp, current_temp


def time_comparator(start_hour, start_minute, stop_hour, stop_minute, current_time):
    start_time = (start_hour*3600 + start_minute*60)*1000
    stop_time = (stop_hour*3600 + stop_minute*60)*1000

    if(start_time < stop_time):
        if(current_time >= start_time and current_time < stop_time):
            return True
        else:
            return False
    elif(start_time >= stop_time):
        if(current_time < start_time and current_time >= stop_time):
            return False
        else:
            return True

def temp_comparator(set_temp, current_temp):
    if(set_temp > current_temp):
        return True
    else:
        return False



class TestBasic:



    @classmethod
    def setup_class(self):
        print('\n===Setup Class===')
        self.fb = firebase.FirebaseApplication('xyz', None)
        self.ard = serial_com.Serial_arduino('COM5')
        self.fb.put('/firebase_test_1', "Mode", "0")
     
    @classmethod
    def teardown_class(self):
        print('\n===Teardown Class===')
        self.fb.put('/firebase_test_1', "Mode", "0")

    def test_current_time(self):
        result = self.ard.read_last_frame()
        expected_current_time = 1000*(time.localtime().tm_hour*3600 + time.localtime().tm_min*60 + time.localtime().tm_sec)
        print(f'Arduino current time: {result.current_time}')
        print(f'Test current time: {expected_current_time}')
        assert abs(expected_current_time - result.current_time) < 5000.0

    def test_receive_frame(self):
        """Wait for 1 frame"""
        self.ard.read_last_frame()
        assert True
    
    @pytest.mark.parametrize("mode",[0, 1, 2, 3])
    def test_set_mode(self, mode):
        """Wait for 1 frame"""
        self.fb.put('/firebase_test_1', "Mode", f"{mode}")
        time.sleep(3)
        result = self.ard.read_last_frame()
        assert result.mode_status == mode

    @pytest.mark.parametrize('set_temp, curr_temp, expected_heater_status', [generate_automatic_temp() for _ in range(10)])
    def test_automatic_mode(self, set_temp, curr_temp, expected_heater_status):
        self.fb.put('/firebase_test_1', "Mode", "2")
        self.fb.put('/firebase_test_1', "Auto_setTemp", f"{set_temp}")
        self.fb.put('/firebase_test_1', "currentTemp", f"{curr_temp}")
        time.sleep(3)
        result = self.ard.read_last_frame()
        assert result.heater_status == expected_heater_status

    @pytest.mark.parametrize('start_hour, start_minute, stop_hour, stop_minute, status', [generate_schedule_time() for _ in range(10)])
    @pytest.mark.parametrize('set_temp, current_temp', [generate_schedule_temp() for _ in range(10)])
    def test_schedule_mode(self,  start_hour, start_minute, stop_hour, stop_minute, status, set_temp, current_temp):
        self.fb.put('/firebase_test_1', "Schedule_setStartHour_1", f'{start_hour}')
        self.fb.put('/firebase_test_1', "Schedule_setStartMinute_1", f'{start_minute}')
        self.fb.put('/firebase_test_1', "Schedule_setStopHour_1", f'{stop_hour}')
        self.fb.put('/firebase_test_1', "Schedule_setStopMinute_1", f'{stop_minute}')
        #to tak na chwile
        status = 1
        ################
        
        self.fb.put('/firebase_test_1', "Schedule_setStatus_1", f'{status}')
        self.fb.put('/firebase_test_1', "Schedule_setTemp_1", f'{set_temp}')
        self.fb.put('/firebase_test_1', "currentTemp", f'{current_temp}')
        time.sleep(3)
        self.fb.put('/firebase_test_1', "Mode", "3")
        time.sleep(3)
        result = self.ard.read_last_frame()

        expected_heater_status = True if (status == 1) and time_comparator(start_hour=start_hour, start_minute=start_minute,
        stop_hour=stop_hour, stop_minute=stop_minute, current_time=result.current_time) and temp_comparator(set_temp=set_temp, current_temp=current_temp) else False
        
        assert result.heater_status == expected_heater_status




