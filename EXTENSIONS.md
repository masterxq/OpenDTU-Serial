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
  - `poll_enabled`: true/false. Ob der regelmäßige Datenabruf für diesen Inverter aktiviert ist.
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
-DUART_DATA_RX=GPIO_NUM_14
-DUART_DATA_TX=GPIO_NUM_15
```

Sobald UART_DATA_TX gesetzt ist wird die funktion aktiviert. Nur empfangen geht nicht.

### Pushing Updates

Damit der speicher nicht auf Wlan angewiesen ist, werden die informationen ebenfalls über die UART Schnittstelle gesendet.
Und zwar immer genau dann wenn eine aktion erfolgreich ungesetzt wurde, also z.B. wenn ein Limit erfolgreich angewendet wurde oder wenn der Inverter Daten sendet.

Die Daten werden als JSON String gesendet, mit einem Zeilenumbruch am Ende. Es gibt also immer genau eine Zeile pro Update.

Es wird unterschieden zwischen zwei Arten von Updates, die sich im JSON Format unterscheiden:

#### Daten Update

Wenn die DTU Daten empfängt, wird ein Update mit genau den live daten aus dem API Endpunkt gesendet. Der timestamp wird dabei weg gelassen weil es immer sofort gesendet wird. Der empfänger kann dann selbst den timestamp setzen nach seiner systemzeit.

- Ein `type: "data"` Update wird nur nach einem erfolgreichen Live-Daten-Request an den Inverter gesendet.
- Das bedeutet der Inverter ist erreichbar und liefert Daten.

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
    "temperature": 45
  }
}
```

#### Limit Update

Wenn der entschluss getroffen wurde ob ein limit cmd erfolgreich angewendet wurde, wird ein Update mit den Informationen zum limit gesendet.
Vorsicht: Meldet beim ersten versuch fehlversuch der übertragung an den Inverter sofort false zurück. Die DTU probiert es weiter bis es erfolgreich ist. Die unmittelbare Negativ antwort ist von daher eigentlich nicht zu gebrauchen.

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

#### on/off/restart

Wenn der entschluss getroffen wurde ob ein on/off/restart cmd erfolgreich angewendet wurde, wird ein Update mit den Informationen zum state gesendet.
Vorsicht: Meldet beim ersten versuch fehlversuch der übertragung an den Inverter sofort false zurück. Die DTU probiert es weiter bis es erfolgreich ist. Die unmittelbare Negativ antwort ist von daher eigentlich nicht zu gebrauchen.

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

#### Polling Update

Wenn die "Daten abrufen" Funktion für einen Inverter ein- oder ausgeschaltet wird, wird ein Update mit den Informationen zum Polling gesendet.
Für das ändern des "Befehle senden" wird kein Update gesendet. Wenn wir das über die serielle schnittstelle ändern, wird eh immer beides geändert.

```json
{
  "type": "polling",
  "serial": "12345678",
  "polling_cmd": {
    "success": true,
    "enabled": false
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

Ein spezialfall ist das wenn polling deaktiviert ist. Dann bekommen Kommandos die an den Inverter gesendet werden ein `success: false` zurück, mit dem zusatz feld polling: false. Das betrifft insbesondere `set_limit` und `set_state`. `get_data` bleibt weiterhin möglich, damit der aktuelle bekannte Zustand inklusive `poll_enabled` weiter abgefragt werden kann. Es wird hierbei auch wieder nicht unterschieden ob daten abrufen oder befehle senden deaktiviert ist. Wenn polling deaktiviert ist, lehnen wir alle Kommandos die an den Inverter gesendet werden, ab.

```json
{
  "type": "ack",
  "command": "set_limit",
  "serial": "12345678",
  "success": false,
  "polling": false
}
```

#### Limit setzen

Immer in Watt, z.B. 250 für 250W

```json
{
  "type": "set_limit",
  "serial": "12345678",
  "watts": 250
}
```

#### Power on/off/restart cmd

```json
{
  "type": "set_state",
  "serial": "12345678",
  "state": "on"
}
```

#### Daten Anfrage

```json
{
  "type": "get_data",
  "serial": "12345678"
}
```

Die DTU antwortet mit einem Datensatz wie oben beschrieben. Wenn man die serial weglässt, antwortet die DTU mit einem Array von Datensätzen für alle Inverter. Dort sind natürlich wieder die ages dabei, weil das ja nicht in Echtzeit ist wie bei den Updates. In beiden fällen (mit oder ohne serial) ist die Antwort im selben Format, nur dass bei der Anfrage mit serial nur ein Objekt im Array zurück kommt und bei der Anfrage ohne serial ein Array von allen Objekten zurück kommt.

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
        "last_state_cmd": "on",
        "poll_enabled": true
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

Wenn polling deaktiviert ist, werden keine live daten mitgeliefert.

#### Datenabfrage eines Inverters deaktivieren

Regelmäßigen Datenabruf für einen Inverter ein- oder ausschalten. Das macht das selbe wie beide schalter in der UI auf an bzw aus zu stellen. (Daten abrufen, Befehle senden). Der kommando wird gebraucht wenn man einen inverter stromlos schaltet, dann werden andere invertern nicht mehr ordentlich aktualisiert, weil die DTU immer auf die Antwort des ausgeschalteten Inverters wartet.

```json
{
  "type": "set_polling",
  "serial": "12345678",
  "enabled": false
}
```


### Zyklische Updates


#### Konfigurierte Inverter und letzte Sichtung

Alle 10 Sekunden sendet die DTU eine Liste aller konfigurierten Inverter mit der Zeit seit dem letzten bestätigten Kontakt. Als bestätigter Kontakt gilt entweder der Erhalt eines Datenpakets vom Inverter oder ein erfolgreich umgesetztes Inverter-Kommando. Das kann genutzt werden um zu erkennen das der Inverter selbst erreicht ist und gibt eine übersicht über die erreichbarkeit aller Inverter.

```json
{
  "type": "configured_inverters",
  "inverters": [
    {
      "serial": "12345678",
      "poll_enabled": true,
      "last_seen_age_ms": 5000
    },
    ...
  ]
}
```
