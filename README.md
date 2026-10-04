# Otonom Araç SIL Karar Motoru (ADAS v2.0)

Bu proje, **Software-in-the-Loop (SIL)** konseptine uygun olarak 4 kişilik yazılım ekibinin paralel çalışabilmesi için tasarlanmış C++ tabanlı bir ADAS karar motoru ve canlı web gösterge paneli altyapısıdır.

---

## 👥 Takım Rolleri ve Görev Dağılımı (4 Kişilik Ekip)

Projede herkes kendi klasör ve modülünden sorumludur. Aşağıdaki tabloya göre görev alanlarınızı belirleyiniz:

| Ekip Üyesi / Rol | Sorumlu Olunan Kodlar & Klasörler | Görev ve Odak Alanları |
| :--- | :--- | :--- |
| **1. Simülasyon Geliştirici**<br>*(Data Generator)* | `include/SensorSimulator.hpp`<br>`src/SensorSimulator.cpp` | • Zamanlayıcı (timer) döngüsünü kurar.<br>• `SensorSimulator` sınıfını yazar.<br>• W tuşu (Gaz) girdisine göre araç hızını ivmelenme/sürtünme fiziğiyle hesaplar.<br>• Yol anomalilerini (çukur/tümsek) ve hız tabelalarını rastgele ürettirir. |
| **2. Karar Algoritmacısı**<br>*(Core Engine)* | `include/AdasController.hpp`<br>`src/AdasController.cpp` | • Fiziksel matematiği koda döker.<br>• Engele ulaşma süresini hesaplayan **Akıllı Süspansiyon** mantığını yazar (Çukur -> %20 yumuşak, Tümsek -> %80 sert, \(t < 1.5s\)).<br>• W tuşunun ezilip acil fren (**AEB & Override**) yapılması gereken `if/else` koşullarını yazar.<br>• Sinyal verildiğinde şerit uyarısını bastıran (**Sinyal Kontrollü LDW - Bilinçli Sollama**) State Machine'i geliştirir. |
| **3. API & Ağ Sorumlusu**<br>*(Network Integration)* | `include/NetworkServer.hpp`<br>`src/NetworkServer.cpp`<br>`include/httplib.h`<br>`include/json.hpp` | • `cpp-httplib` ve `nlohmann/json` başlık dosyalarını kullanarak C++ içinde yerel web sunucusu (localhost:8080) kurar.<br>• Telemetri verilerini JSON'a çevirip dışarı açar (`GET /api/telemetry`).<br>• Arayüzden gelen W tuşu ve Sinyal `POST` taleplerini karşılayıp 1. ve 2. kişinin algoritmalarına iletir. |
| **4. Sistem Entegratörü**<br>*(AI & Frontend)* | `web/index.html`<br>`web/style.css`<br>`web/app.js`<br>`include/Logger.hpp`<br>`src/Logger.cpp`<br>`src/main.cpp` | • Yapay Zeka (LLM) araçlarını yönlendirerek HTML/JS Dashboard arayüzünü geliştirir.<br>• Arayüze Event Listener ekleyerek klavye girdilerinin C++ sunucusuna iletilmesini sağlar.<br>• C++ tarafında `std::ofstream` kütüphanesiyle tüm kararların zaman damgalı `system_log.txt` dosyasına yazdırılmasından sorumludur. |

---

## 📁 Mimari Klasör Yapısı

```text
D:\ADAS-Kontrol-Motoru\
├── CMakeLists.txt                 <- Proje derleme dosyası
├── README.md                      <- Proje ve Görev Dağılım Raporu
├── .gitignore
├── include/                       <- C++ Başlık Dosyaları (Header)
│   ├── SensorSimulator.hpp        [Geliştirici 1]
│   ├── AdasController.hpp         [Geliştirici 2]
│   ├── NetworkServer.hpp          [Geliştirici 3]
│   ├── Logger.hpp                 [Geliştirici 4]
│   ├── httplib.h                  [Kütüphane - Geliştirici 3]
│   └── json.hpp                   [Kütüphane - Geliştirici 3]
├── src/                           <- C++ Kaynak Dosyaları (Source)
│   ├── SensorSimulator.cpp        [Geliştirici 1]
│   ├── AdasController.cpp         [Geliştirici 2]
│   ├── NetworkServer.cpp          [Geliştirici 3]
│   ├── Logger.cpp                 [Geliştirici 4]
│   └── main.cpp                   [Geliştirici 4]
└── web/                           <- Arayüz & Görselleştirme [Geliştirici 4]
    ├── index.html
    ├── style.css
    └── app.js
```

---

## 🗺️ 10 Haftalık Proje Yol Haritası

- **Hafta 1-2 (Altyapı)**: Ortak C++ çalışma ortamının kurulması, GitHub entegrasyonu ve boş projenin derlenmesi.
- **Hafta 3-5 (Simülasyon ve Beyin)**: Sensör simülasyonlarının (`SensorSimulator`) ve AEB/ACC temel karar `if/else` bloklarının (`AdasController`) konsol üzerinde hatasız çalıştırılması.
- **Hafta 6-8 (Haberleşme)**: HTTP API'nin projeye dahil edilmesi (`NetworkServer`), verilerin JSON olarak yayınlanması ve dışarıdan POST komutu dinleme (W tuşu hazırlığı).
- **Hafta 9-10 (Entegrasyon)**: Yapay zeka ile üretilen web arayüzünün API'ye bağlanması, süspansiyon ve LDW testleri, loglama kontrolleri (`system_log.txt`), hata ayıklama ve final sunumu.

---

## 🛠 Derleme ve Çalıştırma Instructions

```bash
# Projeyi derlemek için:
mkdir build
cd build
cmake ..
cmake --build . --config Release

# Çalıştırmak için:
./Release/AdasDecisionEngine.exe
```
