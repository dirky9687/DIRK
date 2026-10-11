# DIRK v0.6.1 LoRa / multi-mesh architecture

## Hardware
DIRK targets the M5Stack Cardputer ADV + Cap LoRa-1262.

- SX1262 NSS: GPIO5
- SX1262 DIO1/IRQ: GPIO4
- SX1262 RESET: GPIO3
- SX1262 BUSY: GPIO6
- SPI: SCK40 / MOSI14 / MISO39
- GPS UART: RX15 / TX13 @ 115200
- PI4IOE5V6408: I2C GPIO8/9, address 0x43, P0 enables the RF antenna switch
- microSD shares the same SPI bus and uses CS12

Never transmit without the LoRa antenna attached.

## Unified mesh UI
v0.6.1 introduces a protocol-aware UI rather than presenting every LoRa/BLE mesh as if it spoke the same wire protocol.

    D = DIRK Mesh
    M = Meshtastic companion node
    C = MeshCore companion node

The LoRa menu provides Nearby Mesh, Chats / Inbox, Contacts, Channels, Positions, Networks, Range Test, Radio Settings and Diagnostics.

Nearby Mesh combines native DIRK SX1262 peers with BLE advertisements from supported companion-radio ecosystems. Favorites are protocol-independent and persist in NVS, with an SD mirror at `/dirk/lora/contacts.csv` when storage is writable.

## DIRK Mesh
DIRK Mesh is the built-in lightweight RadioLib protocol. It can communicate with any LoRa device that implements this documented format; it is not restricted to Cardputers.

Packet format:

    D1|SOURCE_HEX|SEQ|TTL|TYPE|PAYLOAD

Types:
- C broadcast chat
- D direct chat (`DEST_HEX,text`)
- N node/name beacon
- P GPS position
- R range ping
- Q range reply

RadioLib CRC remains enabled. DIRK deduplicates SOURCE+SEQ and may relay packets while TTL is greater than zero. Direct-message packets can be relayed by intermediate DIRK Mesh nodes but are displayed only by the addressed destination.

DIRK Mesh is intentionally separate from Meshtastic and MeshCore. It is not encrypted and must not be presented as compatible traffic for either ecosystem.

## Meshtastic companion discovery
DIRK recognizes the standard Meshtastic BLE service:

    6BA1B218-15A8-461F-9FA8-5DCAE273EAFD

Meshtastic companion nodes can appear in Nearby Mesh and can be saved as contacts. Full native Meshtastic messaging requires the genuine PhoneAPI/protobuf/routing/crypto stack; DIRK does not fabricate those packets.

The repository retains `tools/fetch-meshtastic-adv.sh` to build the genuine Meshtastic ADV sidecar firmware for complete on-air interoperability with Heltec, RAK, T-Beam, Cardputer and other Meshtastic nodes.

## MeshCore companion discovery
DIRK recognizes the MeshCore Companion BLE service:

    6E400001-B5A3-F393-E0A9-E50E24DCCA9E

MeshCore radios can appear in Nearby Mesh and can be favorited into the same persistent Contacts view. Their actual channel/DM transport uses the MeshCore Companion protocol and remains explicitly separate from DIRK Mesh packets.

## Rendering model
The LoRa/chat UI is event-driven in v0.6.1:

- chat redraws on message/input changes rather than every 500 ms;
- node/nearby views redraw when discovery data changes;
- BLE mesh discovery runs at a slower cadence when auto-discovery is enabled;
- LoRa screens are rendered to the existing 240x117 off-screen canvas and pushed as one frame to reduce visible flicker.

## Roadmap
The shared contact/profile/channel data model in v0.6.1 is designed so genuine Meshtastic PhoneAPI and MeshCore Companion transports can plug into the same UI without pretending the incompatible protocols are mutually intelligible over the air.
