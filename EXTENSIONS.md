# Das ist ein Fork der openDTU Firmware, der optimiert ist dazu mit einem AC-Speicher zusammen zu arbeiten

Die queue für limit cmds wurde auf 1 beschränkt. Wenn ein neues Limit gesetzt wird, wird der alte cmd überschrieben.
Das ist so weil es meist das ist was man will.

## Extra API Endpunkt

Ein Array mit diesen informationen für jeden Inverter ist erreichbar unter `api/v1/status`.
Einzelne Inverter können mit `api/v1/status?serial=<SERIAL>` abgefragt werden, dann erhällt man nur das entsprechende Objekt, wobei SERIAL die ID des Inverters ist.

Enthällt Folgende Datensätze:

- `serial`: ID des Inverters
- `limit_cmd`: Objekt mit den Informationen zum letzten Limit
  - `watts_applied`: Das letzte Limit in Watt das erfolgreich gesetzt wurde.
  - `last_applied_age_ms`: Zeit seit dem letzten angewendeten Limit in ms. Bezieht sich auf erfolgreiche limits.
  - `last_result`: ok | failure | never. Ob das letzte Limit erfolgreich angewendet wurde oder noch nie eines angewendet wurde. Gilt nur für limit cmds! Nicht für on/off/restart usw.
  - `pending_limit`: Ob gerade ein Limit noch auf die umsetzung wartet.
  - `watts_pending`: Das Limit in Watt das gerade auf die Umsetzung wartet. Weglassen wenn `pending_limit` false ist.
  - `last_state_cmd`: on/off. gibt an welcher state (Anschalten/Ausschalten) zuletzt erfolgreich an den Inverter gesendet wurde.
- `live`: Objekt mit den aktuellen Live Daten des Inverters
  - `live_age_ms`: Zeit seit den letzten empfangenen inverter Datenpaket in ms
  - `ac_power`: Aktuelle Leistung in Watt auf netz Seite
  - `ac_voltage`: Aktuelle Spannung in Volt auf netz Seite
  - `ac_current`: Aktueller Strom in Ampere auf netz Seite
  - `dc_power`: Aktuelle Leistung in Watt auf PV Seite
  - `dc_voltage`: Aktuelle Spannung in Volt auf PV Seite
  - `dc_current`: Aktueller Strom in Ampere auf PV Seite
  - `temperature`: Aktuelle Temperatur in Grad Celsius
  - `reachable`: true/false. Ob der inverter uns daten liefert, std opendtu timeout.

## Uart Interface

Baud: 115200
8N1

Setting in der buildconf:

```bash
-DUART_DATA_RX=GPIO_NUM_15
-DUART_DATA_TX=GPIO_NUM_14
```

Sobald UART_DATA_TX gesetzt ist wird die funktion aktiviert. Nur empfangen geht nicht.

### Pushing Updates

Damit der speicher nicht auf Wlan angewiesen ist, werden die informationen ebenfalls über die UART Schnittstelle gesendet.
Und zwar immer genau dann wenn eine aktion erfolgreich ungesetzt wurde, also z.B. wenn ein Limit erfolgreich angewendet wurde oder wenn der Inverter Daten sendet.

Die Daten werden als JSON String gesendet, mit einem Zeilenumbruch am Ende. Es gibt also immer genau eine Zeile pro Update.

Es wird unterschieden zwischen zwei Arten von Updates, die sich im JSON Format unterscheiden:

- Daten Update: Wenn die DTU Daten empfängt, wird ein Update mit genau den live daten aus dem API Endpunkt gesendet. Der timestamp wird dabei weg gelassen weil es immer sofort gesendet wird. Der empfänger kann dann selbst den timestamp setzen nach seiner systemzeit.

```json
{
  "type": "data",
  "serial": "12345678",
  "live": {
    "ac_power": 3000,
    "ac_voltage": 230,
    "ac_current": 13,
    "dc_power": 3200,
    "dc_voltage": 400,
    "dc_current": 8,
    "temperature": 45,
    "reachable": true
  }
}
```

- Limit Update: Wenn der entschluss getroffen wurde ob ein limit cmd erfolgreich angewendet wurde, wird ein Update mit den Informationen zum limit gesendet.

```json
{
  "type": "limit",
  "serial": "12345678",
  "limit_cmd": {
    "success": true,
    "watts_applied": 2500
  }
}
```

- Ähnliches bei einem power on/off/restart cmd:

```json
{
  "type": "state",
  "serial": "12345678",
  "state_cmd": {
    "success": true,
    "command": "restart",
    "state": "on"
  }
}
```

Bei `set_state` darf `state` die Werte `on`, `off` oder `restart` haben.
Bei einem `state` Push Update ist `command` das tatsächlich ausgeführte Kommando: `on`, `off` oder `restart`.
`state` beschreibt den resultierenden Inverterzustand und ist daher immer nur `on` oder `off`.
Ein erfolgreich umgesetzter `restart` wird also als `command: "restart"` und `state: "on"` gemeldet.

### Daten Anfrage und Kommandos absetzen

Man kann auch Daten an die DTU senden, um entweder Informationen abzufragen oder Kommandos zu setzen.
Auch hier gilt dass jedes json Objekt mit einem Zeilenumbruch am Ende gesendet wird.

Es wird empfohlen immer nur ein Kommando zu senden und dann auf ein `ack` zu warten. Dort ist 300ms timeout ein guter richtwert.

Jedes empfangene Kommando wird zuerst mit einem `ack` bestätigt. Dieses `ack` bestätigt nur dass die DTU das Kommando empfangen und verarbeitet hat. Ob der Inverter das Kommando später erfolgreich umsetzt, wird weiterhin asynchron über ein `limit`, `state` oder `requested_data` JSON gemeldet.

```json
{
  "type": "ack",
  "command": "set_limit",
  "serial": "12345678",
  "success": true
}
```

- Limit setzen. Immer in Watt, z.B. 250 für 250W

```json
{
  "type": "set_limit",
  "serial": "12345678",
  "watts": 250
}
```

- Power on/off/restart cmd

```json
{
  "type": "set_state",
  "serial": "12345678",
  "state": "on"
}
```

- Daten Anfrage

```json
{
  "type": "get_data",
  "serial": "12345678"
}
```

Die DTU antwortet mit einem Datensatz wie oben beschrieben. Wenn man die serial weglässt,antwortet die DTU mit einem Array von Datensätzen für alle Inverter. Dort sind natürlich wieder die ages dabei, weil das ja nicht in Echtzeit ist wie bei den Updates. In beiden fällen (mit oder ohne serial) ist die Antwort im selben Format, nur dass bei der Anfrage mit serial nur ein Objekt im Array zurück kommt und bei der Anfrage ohne serial ein Array von allen Objekten zurück kommt.

```json
{
  "type": "requested_data",
  "data": [
    {
      "limit_cmd": {
        "watts_applied": 2500,
        "last_applied_age_ms": 5000,
        "last_result": "ok",
        "pending_limit": false,
        "last_state_cmd": "on"
      },
      "serial": "12345678",
      "live": {
        "live_age_ms": 2000,
        "ac_power": 3000,
        "ac_voltage": 230,
        "ac_current": 13,
        "dc_power": 3200,
        "dc_voltage": 400,
        "dc_current": 8,
        "temperature": 45,
        "reachable": true
      }
    },
    ...
  ]
}
```
