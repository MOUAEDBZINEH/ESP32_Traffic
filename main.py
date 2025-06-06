import cv2
import numpy as np
import requests
from tensorflow.keras.models import load_model
import time

# ✅ تحميل نموذج التعلم العميق المدرب مسبقًا
model = load_model("cnn_Traffic_Signs.h5")
class_names = ['GO', 'STOP', 'PEDESTRIAN', 'RIGHT_TURN', 'U_TURN', 'NULL']
IMG_SIZE = 64

# ✅ عنوان ESP32-CAM عند استخدامه كنقطة وصول (SoftAP)
ESP32_IP = "http://192.168.4.1"

# ✅ دالة لجلب صورة من ESP32-CAM
def get_image():
    try:
        response = requests.get(f"{ESP32_IP}/capture", timeout=3)
        if response.status_code == 200:
            img_array = np.asarray(bytearray(response.content), dtype=np.uint8)
            frame = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
            return frame
        else:
            print("[❌] فشل في الحصول على الصورة من ESP32-CAM.")
            return None
    except:
        print("[⚠️] لم يتم الاتصال بـ ESP32.")
        return None

# ✅ دالة لتصنيف الصورة المأخوذة ثم إرسال النتيجة إلى ESP32
def classify_and_send(frame):
    # تجهيز الصورة للشبكة العصبية
    img = cv2.resize(frame, (IMG_SIZE, IMG_SIZE))
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    img = img.astype("float32") / 255.0
    img = np.expand_dims(img, axis=0)

    # التنبؤ بالإشارة
    predictions = model.predict(img)
    class_id = np.argmax(predictions)
    confidence = predictions[0][class_id]
    result_class = class_names[class_id]

    print(f"[✔️] تم التعرف على: {result_class} (دقة: {confidence:.2f})")

    # إرسال النتيجة إلى ESP32 فقط في حالة GO (تشغيل المحرك)
    try:
        url = f"{ESP32_IP}/control?cmd=serial;{result_class};{confidence:.2f};stop"
        requests.get(url, timeout=2)
        print("[📤] تم إرسال النتيجة إلى ESP32-CAM.")
    except:
        print("[⚠️] فشل إرسال النتيجة إلى ESP32.")

    return result_class, confidence

# ✅ حلقة التشغيل الرئيسية
while True:
    frame = get_image()
    if frame is not None:
        cv2.imshow("ESP32-CAM Stream", frame)  # عرض الصورة
        classify_and_send(frame)

    # للخروج من البرنامج اضغط 'q'
    if cv2.waitKey(1000) & 0xFF == ord('q'):
        break

cv2.destroyAllWindows()
