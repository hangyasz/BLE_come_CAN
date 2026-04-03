# BLE Párosítási Rendszer - Autó Fejegység Típusú

## Rendszer Áttekintése

Ez egy biztonságos BLE párosítási rendszer, amely autó fejegységekhez hasonlóan működik:

### Fő Funkciók

1. **PIN-alapú Párosítás** - Random 6 jegyű PIN kód az első csatlakozáskor
2. **Szelektív Hirdetés** - Az eszköz csak párosítási módban látható a keresésben
3. **Kötelezettség Tárolása** - Párosított eszközök automatikus felismerése
4. **Biztonsági Protokoll** - MITM Protection és kötelezettség alapú hitelesség

---

## Párosítási Folyamat

### 1️⃣ Párosítási Mód Aktiválása (ESP32)
```
Soros port parancs: START_PAIRING
```
- Az ESP32 generál egy random 6 jegyű PIN kódot
- A PIN megjelenik a soros monitornál
- Az eszköz 5 percig **kereshető** lesz BLE keresésben

### 2️⃣ Telefon: Eszköz Keresése
- Nyisd meg a BLE keresőt
- Egy új eszköz fog megjelenni: **"CarHead"**
- (Más párosított eszközök nem látszódnak a keresésben)

### 3️⃣ Csatlakozás és PIN Beírása
- Csatlakozz az eszközhöz
- Az alkalmazás kérni fog a PIN kódra
- Írja be, amit a soros porton lát: `XXXXXX`

### 4️⃣ Párosítás Befejezése
- Ha a PIN helyes: **"PAIRED"** státusz
- Az eszköz **kötelezettséget** ment
- Az eszköz neveként mentésre kerül: **"CarHead"**

### 5️⃣ Következő Csatlakozások
- A párosított telefonok **automatikusan** felismerik az eszközt
- Az eszköz **nem lesz látható** a keresésben
- Közvetlen csatlakozás a szokottak alapján a Bluetooth beállításokból

---

## Soros Port Parancsok

### 🔧 Párosítási Mód Vezérlés

| Parancs | Hatás |
|---------|-------|
| `START_PAIRING` | Párosítási mód aktiválása (5 perc) |
| `STOP_PAIRING` | Párosítási mód deaktiválása |
| `STATUS` | Rendszer státusza és párosított ezközök listája |
| `DELETE_BOND <ADDRESS>` | Egy eszköz kötelezettségének törlése |
| `DELETE_ALL_BONDS` | Összes kötelezettség törlése |

### Parancs Például

```
START_PAIRING
[PAIRING] Párosítási mód aktív! PIN: 123456
[PAIRING] Az eszköz 5 percig kereshető lesz...

STATUS
[STATUS] Párosítási mód: AKTÍV
[STATUS] Párosított eszközök száma: 1
  - aa:bb:cc:dd:ee:ff

DELETE_ALL_BONDS
[DELETE] Minden kötelezettség törölve!
```

---

## BLE Karakterisztikák

### 🔑 PIN Karakterisztika
- **UUID**: `beb5483e-36e1-4688-b7f5-ea07361b26a8`
- **Műveletek**: Olvasás (PIN lekérés), Írás (PIN ellenőrzés)
- **Leírás**: Az ESP32 generál a PIN-t, a telefon pedig elküldi az ellenőrzéshez

### 📊 Státusz Karakterisztika
- **UUID**: `beb5483e-36e1-4688-b7f5-ea07361b26a9`
- **Műveletek**: Olvasás, Értesítés
- **Lehetséges Értékek**:
  - `PAIRING_ACTIVE` - Párosítási mód futó
  - `PAIRING_INACTIVE` - Párosítási mód kikapcsolt
  - `PAIRED` - Sikeres párosítás
  - `INVALID_PIN` - Hibás PIN

---

## Biztonsági Beállítások

```cpp
BLE_SEC_BONDING = true      // Kötelezettség (persistent pairing)
BLE_SEC_MITM = true         // MITM Protection (tüzfal)
BLE_SEC_SC = false          // Secure Connections (nem szükséges)
IO_CAP = DISPLAY_ONLY       // PIN megjelenítés csak
```

---

## NVS Tárolás (Persistent Memory)

Az NVS flash memóriában tárolt párosított eszközök:

```
NVS Namespace: "bonded_devs"
Kulcs formátum: "dev_<MAC_ADDRESS>"
Érték: "CarHead"
```

### Tárolás Strukturája

```
bonded_devs/
  - dev_aa:bb:cc:dd:ee:ff = "CarHead"
  - dev_11:22:33:44:55:66 = "CarHead"
```

---

## Telepítés és Fordítás

```bash
# Projekt építés
platformio run

# Feltöltés ESP32-re
platformio run --target upload

# Soros monitor (hibakeresés)
platformio device monitor
```

---

## Fejlesztéshez: Android Alkalmazás Integráció

Ha saját app-ot írasz, használd ezeket az UUID-ket:

```kotlin
val SERVICE_UUID = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b")
val CHAR_UUID_PIN = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8")
val CHAR_UUID_STATUS = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a9")
```

### Párosítás Flow (Android)

```kotlin
// 1. Keresés elindítása
startScan()  // Megjelenik a "CarHead"

// 2. Csatlakozás
connect(device)

// 3. PIN lekérés
characteristic.read(CHAR_UUID_PIN)  // -> "123456"

// 4. PIN ellenőrzés
characteristic.write(CHAR_UUID_PIN, "123456")

// 5. Státusz ellenőrzése
characteristic.enableNotification(CHAR_UUID_STATUS)
// -> callback: "PAIRED"
```

---

## Problémamegoldás

### 🔴 Az eszköz nem látszódik a keresésben
- Ellenőrizd, hogy a `START_PAIRING` parancs futott-e
- Ellenőrizd a soros porton az üzeneteket: `[ADV] Hirdetés elindult`

### 🔴 PIN hibás
- Győződj meg, hogy a soros monitorban látható PIN-t írod be
- A PIN generálódása random, időnként más

### 🔴 Párosítás nem marad meg
- Ellenőrizd, hogy az NVS inicializálódott-e: `[NVS] Eszköz mentve`
- Ha nem működik, próbáld a `DELETE_ALL_BONDS` és újra párosítsd

### 🔴 Egy telefon többször párosít
- Így működik értelmesen: az eszköz emlékszik az eszközre
- Ha újra párosítanál, először töröld: `DELETE_BOND`

---

## Konfiguráció (include/ble_config.h)

Szükség esetén módosítható értékek:

```cpp
#define DEVICE_NAME "CarHead"        // Eszköz neve
#define PAIRING_TIMEOUT 300000       // 5 perc párosítási mód
#define PIN_LENGTH 6                 // 6 jegyű PIN kód
```

---

## Logok Olvasása

A soros port ezeket az üzeneteket mutatja:

```
[BLE] Szerver inicializálva
[PAIRING] Párosítási mód aktív! PIN: 123456
[ADV] Hirdetés elindult
[BLE] Csatlakozva! Peer Address: aa:bb:cc:dd:ee:ff
[PIN] Fogadott adat: 123456
[SECURITY] PIN helyes! Párosítás sikeres!
[NVS] Eszköz mentve: aa:bb:cc:dd:ee:ff -> CarHead
```

---

## Verzió Információ

- **NimBLE-Arduino**: v2.1.0+
- **ESP32**: DevKit v1 vagy kompatibilis
- **Arduino Framework**
- **Dátum**: 2026. április

---

Valahányszor `START_PAIRING` parancsot adsz ki, az ESP32 **5 percig látható** a keresésben,
majd automatikusan rejtődik. Ez biztosítja, hogy csak az építkező új eszközök párosodnak.
