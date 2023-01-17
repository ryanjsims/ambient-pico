import threading
import random
import time
from flask_socketio import SocketIO, disconnect

from flask import Flask, request

app = Flask(__name__)
sio = SocketIO(app)
app.config['SECRET_KEY'] = 'secret!'

# @sio.event
# def connect(sid):
#     print('connect ', sid)

# @sio.event
# def disconnect(sid):
#     print('disconnect ', sid)

@sio.event
def subscribe(data):
    print("Subscription added ", data)
    threading.Timer(60, emit_data).start()
    sio.emit("subscribed", {
        "devices": [
            {
                "apiKey": "68fe58a00d4a464780f79435db775a1c4f92fcaaca2b46d48bf87e71ca092fb8",
                "info": {
                    "coords": {
                        "address": "7829 N Maiden Pools Pl, Tucson, AZ 85743, USA",
                        "coords": {
                            "lat": 32.348947,
                            "lon": -111.117926
                        },
                        "elevation": 668.9186401367188,
                        "geo": {
                            "coordinates": [
                                -111.117926,
                                32.348947
                            ],
                            "type": "Point"
                        },
                        "location": "Tucson"
                    },
                    "name": "Weather"
                },
                "lastData": {
                    "baromabsin": 27.628,
                    "baromrelin": 29.799,
                    "batt1": 1,
                    "batt2": 1,
                    "batt_co2": 1,
                    "battout": 1,
                    "dailyrainin": 0.008,
                    "date": "2023-01-15T22:25:00.000Z",
                    "dateutc": 1673821500000,
                    "deviceId": "63ab62151898f0000124f06f",
                    "dewPoint": 49.86,
                    "dewPointin": 44.1,
                    "feelsLike": 55.6,
                    "feelsLikein": 70.4,
                    "hourlyrainin": 0,
                    "humidity": 81,
                    "humidityin": 37,
                    "lastRain": "2023-01-15T19:42:00.000Z",
                    "maxdailygust": 14.76,
                    "monthlyrainin": 0.228,
                    "solarradiation": 21.99,
                    "temp1f": 53.4,
                    "temp2f": 53.8,
                    "tempf": 55.6,
                    "tempinf": 71.8,
                    "totalrainin": 0.28,
                    "tz": "America/Phoenix",
                    "uv": 0,
                    "weeklyrainin": 0.008,
                    "winddir": 317,
                    "windgustmph": 5.82,
                    "windspeedmph": 4.47
                },
                "macAddress": "30:83:98:A6:AE:AA"
            }
        ],
        "method": "subscribe"
    })
    time.sleep(10)
    print(f"disconnecting {request.sid}")
    disconnect()

def emit_data():
    print("sending data")
    threading.Timer(60, emit_data).start()
    sio.emit("data", {
        "baromabsin": 27.611,
        "baromrelin": 29.781,
        "batt1": 1,
        "batt2": 1,
        "batt_co2": 1,
        "battout": 1,
        "dailyrainin": 0.028,
        "date": "2023-01-15T22:52:00.000Z",
        "dateutc": 1673823120000,
        "dewPoint": 50.01,
        "dewPointin": 44.6,
        "feelsLike": 54.1,
        "feelsLikein": 70.2,
        "hourlyrainin": 0.047,
        "humidity": 86,
        "humidityin": 38,
        "lastRain": "2023-01-15T22:52:00.000Z",
        "macAddress": "30:83:98:A6:AE:AA",
        "maxdailygust": 14.76,
        "monthlyrainin": 0.248,
        "solarradiation": 73.9,
        "temp1f": 53.6,
        "temp2f": 53.8,
        "tempf": random.random() * 25.0 + 50.0,
        "tempinf": 71.6,
        "totalrainin": 0.299,
        "tz": "America/Phoenix",
        "uv": 0,
        "weeklyrainin": 0.028,
        "winddir": 215,
        "windgustmph": 2.24,
        "windspeedmph": 1.79
    })

if __name__ == "__main__":
    sio.run(app, "0.0.0.0", 8000)