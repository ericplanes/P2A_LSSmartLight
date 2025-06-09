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

### **Mòduls Planificats**

```
TLights.c/h      - Control PWM 6 llums (2 HW + 4 SW)
TRFID.c/h        - Comunicació SPI cooperativa amb RFID-RC522
TKeypad.c/h      - Lectura teclat matricial 3x4
TLcd.c/h         - Gestió display LCD
TUserConfig.c/h  - Emmagatzematge configuracions usuaris (EEPROM)
TSerial.c/h      - Comunicació sèrie amb ordinador
TController.c/h  - Màquina d'estats principal
Utils.h          - Definicions tipus i constants
```

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

### **✅ Visual Studio Code (Recomanat)**

Plugins necessaris:

- **C/C++** de Microsoft
- **Makefile Tools** (opcional)

### **Configuració Inclosa**

- `.vscode/c_cpp_properties.json` - IntelliSense per PIC18F4321
- `.vscode/settings.json` - Configuració editor C
- `.vscode/tasks.json` - Tasca build integrada

**📝 Nota**: El path del compilador està configurat per macOS. Adjusta segons el teu sistema:

```json
"/Applications/microchip/xc8/v3.00/pic/include"
```

---

## 🚀 Compilació i Programació

### **Compilar Projecte**

```bash
make
```

### **Netejar Build**

```bash
make clean
```

### **Programar Microcontrolador**

1. Compila el projecte per generar `.hex`
2. Obre **MPLAB IPE**
3. Selecciona dispositiu `PIC18F4321`
4. Carrega el `.hex` i programa

---

## 📁 Estructura del Projecte

```
P2A_LSSmartLight.X/
├── .vscode/                 # Configuració VS Code
├── nbproject/               # Configuració MPLAB X
├── main.c                   # Punt entrada aplicació
├── Utils.h                  # Definicions tipus globals
├── Makefile                 # Build configuration
├── README.md                # Aquest document
├── .gitignore              # Fitxers exclosos git
├── Enunciat-P2FA.txt       # Especificacions projecte
└── Datasheet PIC18F4321.txt # Documentació tècnica
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

## 🎯 Estat Implementació

- [x] **Estructura base projecte**
- [x] **Configuració build i IDE**
- [x] **Configuració microcontrolador**
- [ ] **Mòdul control llums PWM**
- [ ] **Driver RFID cooperatiu**
- [ ] **Interfície teclat matricial**
- [ ] **Control display LCD**
- [ ] **Gestió configuracions usuaris**
- [ ] **Comunicació sèrie**
- [ ] **Controlador principal**
- [ ] **Testing i validació**

---

## 💼 Bones Pràctiques

- 🌟 Utilitza **branches i Pull Requests** per desenvolupar mòduls
- 🤝 Coordina fitxers compartits amb l'equip
- 📝 Documenta canvis significatius
- 🧪 Testa cada mòdul independentment
- ⚡ Optimitza ús memòria RAM
- 🔄 Utilitza sistemes cooperatius (no bloquejants)

---

## 📚 Referències Tècniques

- **PIC18F4321 Datasheet**: [Datasheet PIC18F4321.txt](./Datasheet%20PIC18F4321.txt)
- **Especificacions Projecte**: [Enunciat-P2FA.txt](./Enunciat-P2FA.txt)
- **RFID-RC522**: Documentació disponible a estudy
- **XC8 Compiler**: Microchip Development Tools

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
