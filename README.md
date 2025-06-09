# P2A_LSSmartLight

**Pràctica 2 – Fase A**  
Sistema de control lumínic programable per sala de reunions  
Basat en microcontrolador **PIC18F4321**, en llenguatge **C cooperatiu**

---

## 🎯 Objectiu del Projecte

Dissenyar i construir un sistema de control lumínic intel·ligent per una sala de reunions amb **6 llums**. El sistema detecta automàticament quan un usuari entra o surt de la sala mitjançant targetes RFID i configura la il·luminació segons les preferències personalitzades de cada usuari.

---

## 🔧 Especificacions Tècniques

### **Microcontrolador**

- **Model**: PIC18F4321 (40 pins)
- **Memòria Flash**: 8KB (4096 instruccions)
- **RAM**: 512 bytes
- **EEPROM**: 256 bytes
- **Freqüència**: 32 MHz (8 MHz + 4x PLL)

### **Perifèrics del Sistema**

- 🔗 **Canal sèrie** - Comunicació amb ordinador
- 📟 **Mòdem RFID-RC522** - Lectura targetes usuaris (SPI cooperatiu)
- ⌨️ **Teclat matricial 3x4** - Interacció usuari
- 🖥️ **Display LCD** - Informació estat sistema
- 💡 **6 Llums PWM** - Control intensitat (50Hz, 11 nivells: 0x0-0xA)

---

## ⚡ Funcionalitats Principals

### **Control RFID**

- **Entrada usuari** → Activació llums amb configuració personalitzada
- **Sortida usuari** → Apagada automàtica de tots els llums
- **Canvi usuari** → Actualització immediata configuració

### **Control Manual (Teclat)**

- `[Llum][Intensitat]` → Configuració directa (ex: `3` + `7` = Llum 3 al 70%)
- `[Llum]` + `*` → Intensitat màxima
- `#` (3s) → Reset configuracions tots usuaris
- `#` (<3s) → Menú principal

### **Interfície Ordinador**

1. **Qui hi ha a la sala?** - Mostra usuari actual
2. **Mostrar configuracions** - Llista configuracions usuaris
3. **Modificar hora sistema** - Actualització hora

### **Display LCD**

Format: `"F 16:30 1-0 2-3 3-3 4-0 5-9 6-A"`

- Últim caràcter UID usuari actual
- Hora actual del sistema
- Estat individual dels 6 llums

---

## 🏗️ Arquitectura del Sistema

### **Arquitectura Modular**

Sistema organitzat en mòduls cooperatius independents:

- **TLights**: Control PWM 6 llums (2 HW + 4 SW)
- **TRFID**: Comunicació SPI cooperativa amb RFID-RC522
- **TKeypad**: Lectura teclat matricial 3x4
- **TLcd**: Gestió display LCD
- **TUserConfig**: Emmagatzematge configuracions usuaris (EEPROM)
- **TSerial**: Comunicació sèrie amb ordinador
- **TController**: Màquina d'estats principal
- **Utils**: Definicions tipus i constants compartides

### **Assignació Pins (Planificada)**

- **6 pins** → Sortides PWM llums
- **4 pins** → SPI RFID (SCK, MOSI, MISO, CS)
- **7 pins** → Teclat 3x4 (3 files + 4 columnes)
- **6 pins** → LCD (RS, E, D4-D7)
- **2 pins** → Comunicació sèrie (TX, RX)

### **Reptes Tècnics**

- **⚠️ Memòria limitada**: Només 512 bytes RAM total
- **PWM múltiples**: 2 HW + 4 SW PWM per 6 llums
- **SPI cooperatiu**: Implementació bit-banging obligatòria
- **Gestió usuaris**: Màxim usuaris possibles amb memòria disponible

---

## 🛠️ Configuració Entorn Desenvolupament

### **✅ MPLAB X IDE (Principal)**

- **Versió recomanada**: MPLAB X IDE v6.00 o superior
- **Compilador**: XC8 v2.46 o superior
- **Dispositiu**: PIC18F4321
- **Compatible**: Windows, macOS, Linux

### **Configuració Cross-Platform**

El projecte està configurat per funcionar en diferents sistemes operatius:

- **Windows**: Path automàtic del compilador XC8
- **macOS**: Path automàtic del compilador XC8
- **Configuracions compartides**: `project.xml` i `configurations.xml`

### **Desenvolupament en Equip (Mac + Windows)**

- **Equip mixt**: macOS i Windows treballant simultàniament
- **Sincronització**: Git manté compatibilitat entre plataformes
- **Configuracions locals**: Cada desenvolupador manté les seves preferències

---

## 🚀 Compilació i Programació

### **Obrir Projecte**

1. Obre **MPLAB X IDE**
2. `File` → `Open Project`
3. Selecciona la carpeta `P2A_LSSmartLight.X`
4. El projecte es carregarà automàticament

### **Compilar Projecte**

- **Build**: `Production` → `Build Main Project` (F11)
- **Clean & Build**: `Production` → `Clean and Build Main Project` (Shift+F11)

### **Programar Microcontrolador**

1. Connecta el programador/debugger (PICkit, ICD, etc.)
2. **Program**: `Production` → `Make and Program Device Main Project` (F5)
3. **Debug**: `Debug` → `Debug Main Project` (Ctrl+F5)

---

## 📁 Estructura del Projecte

```
P2A_LSSmartLight.X/
├── nbproject/               # Configuració MPLAB X
│   ├── project.xml          # Configuració projecte (compartida)
│   └── configurations.xml   # Configuració target (compartida)
├── main.c                   # Punt entrada aplicació
├── Utils.h                  # Definicions tipus globals
├── Makefile                 # Build configuration
└── README.md                # Aquest document
```

### **Mòduls a Desenvolupar**

```
├── TLights.c/h             # Control PWM 6 llums
├── TRFID.c/h               # Comunicació SPI cooperativa RFID-RC522
├── TKeypad.c/h             # Lectura teclat matricial 3x4
├── TLcd.c/h                # Gestió display LCD
├── TUserConfig.c/h         # Emmagatzematge configuracions (EEPROM)
├── TSerial.c/h             # Comunicació sèrie ordinador
└── TController.c/h         # Màquina d'estats principal
```

---

## 📊 Restriccions i Consideracions

### **Memòria RAM (512 bytes total)**

- Variables globals sistema: ~100 bytes
- Stack i temporals: ~100 bytes
- **Disponible usuaris**: ~300 bytes
- **Estimació**: 10 bytes/usuari → **~30 usuaris màxim**

### **PWM (6 sortides requerides)**

- **Hardware**: 2 canals (CCP1, CCP2)
- **Software**: 4 canals addicionals via Timer0
- **Freqüència**: 50Hz obligatori
- **Resolució**: 11 nivells (0x0 a 0xA)

### **SPI Cooperatiu**

- ❌ **Prohibit** usar mòdul MSSP hardware
- ✅ **Obligatori** implementació bit-banging
- Temporització crítica per RFID-RC522

---

## 💼 Bones Pràctiques

- 🌟 Utilitza **branches i Pull Requests** per desenvolupar mòduls
- 🤝 Coordina fitxers compartits amb l'equip
- 📝 Documenta canvis significatius
- 🧪 Testa cada mòdul independentment
- ⚡ Optimitza ús memòria RAM
- 🔄 Utilitza sistemes cooperatius (no bloquejants)

---

## ✨ Autors

- **Nom**: Eric Planes i Francesc Tur
- **Universitat**: La Salle – Universitat Ramon Llull
- **Curs**: Sistemes Digitals i Microprocessadors (2024-2025)
- **Professor**: [Nom Professor]

---

## 📄 Llicència

Aquest projecte està desenvolupat com a material acadèmic per l'assignatura de Sistemes Digitals i Microprocessadors de La Salle URL.

---

_Projecte generat amb ❤️ i molt cafè ☕ durant les pràctiques de SDM 2024-2025_
