
 /*-O//\             __     __
   |-gfo\           |__| | |  | |\ | ™
   |!y°o:\          |  __| |__| | \| v6.0
   |y"s§+`\         multi-master, multi-media communications bus system framework
  /so+:-..`\        Copyright 2010-2016 by Giovanni Blu Mitolo gioscarab@gmail.com
  |+/:ngr-*.`\
  |5/:%&-a3f.:;\
  \+//u/+g%{osv,,\
    \=+&/osw+olds.\\
       \:/+-.-°-:+oss\
        | |       \oy\\
        > <
 ______-| |-___________________________________________________________________

PJON™ is a self-funded, no-profit project created and mantained by Giovanni Blu Mitolo
with the support ot the internet community if you want to see the PJON project growing
with a faster pace, consider a donation at the following link: https://www.paypal.me/PJON

PJON™ Protocol specification:
- v0.1 https://github.com/gioblu/PJON/blob/master/specification/PJON-protocol-specification-v0.1.md
- v0.2 https://github.com/gioblu/PJON/blob/master/specification/PJON-protocol-specification-v0.2.md
- v0.3 https://github.com/gioblu/PJON/blob/master/specification/PJON-protocol-specification-v0.3.md
- v1.0 https://github.com/gioblu/PJON/blob/master/specification/PJON-protocol-specification-v1.0.md

PJON™ Acknowledge specification:
- v0.1 https://github.com/gioblu/PJON/blob/master/specification/PJON-protocol-acknowledge-specification-v0.1.md

PJON™ Dynamic addressing specification:
- v0.1 https://github.com/gioblu/PJON/blob/master/specification/PJON-dynamic-addressing-specification-v0.1.md

PJON™ Standard compliant tools:
- https://github.com/aperepel/saleae-pjon-protocol-analyzer Logic analyzer by Andrew Grande
- https://github.com/Girgitt/PJON-python PJON running on Python by Zbigniew Zasieczny

Credits to contributors:
- Fred Larsen (Systems engineering, header driven communication, debugging)
- Pantovich github user (update returning number of packets to be delivered)
- Adrian Sławiński (Fix to enable SimpleModbusMasterV2 compatibility)
- SticilFace github user (Teensy porting)
- Esben Soeltoft (Arduino Zero porting)
- Alex Grishin (ESP8266 porting)
- Andrew Grande (Testing, support, bugfix)
- Mauro Zancarlin (Systems engineering, testing, bugfix)
- Michael Teeww (Callback based reception, debugging)
- PaoloP74 github user (Library conversion to 1.x Arduino IDE)

Bug reports:
- Zbigniew Zasieczny (header reference inconsistency report)
- DanRoad reddit user (can_start ThroughSerial bugfix)
- Remo Kallio (Packet index 0 bugfix)
- Emanuele Iannone (Forcing SIMPLEX in OverSamplingSimplex)
- Christian Pointner (Fixed compiler warnings)
- Andrew Grande (ESP8266 example watchdog error bug fix)
- Fabian Gärtner (receive function and big packets bugfix)
- Mauro Mombelli (Code cleanup)
- Shachar Limor (Blink example pinMode bugfix)
 ______________________________________________________________________________

Copyright 2012-2016 by Giovanni Blu Mitolo gioscarab@gmail.com

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#ifndef PJON_h
  #define PJON_h
  #include <Arduino.h>
  #include <PJONDefines.h>
  #include "strategies/EthernetTCP/EthernetTCP.h"
  #include "strategies/LocalUDP/LocalUDP.h"
  #include "strategies/OverSampling/OverSampling.h"
  #include "strategies/SoftwareBitBang/SoftwareBitBang.h"
  #include "strategies/ThroughSerial/ThroughSerial.h"

  template<typename Strategy = SoftwareBitBang>
  class PJON {
    public:
      Strategy strategy;

      /* PJON bus default initialization:
         State: Local (bus_id: 0.0.0.0)
         Acknowledge: true (Acknowledge is requested)
         device id: NOT_ASSIGNED (255)
         Mode: HALF_DUPLEX
         Sender info: true (Sender info are included in the packet)
         Strategy: SoftwareBitBang */

      PJON() : strategy(Strategy()) {
        _device_id = NOT_ASSIGNED;
        set_default();
      };


      /* PJON initialization passing device id:
         PJON bus(1); */

      PJON(uint8_t device_id) : strategy(Strategy()) {
        _device_id = device_id;
        set_default();
      };


      /* PJON initialization passing bus and device id:
         uint8_t my_bus = {1, 1, 1, 1};
         PJON bus(my_bys, 1); */

      PJON(const uint8_t *b_id, uint8_t device_id) : strategy(Strategy()) {
        copy_bus_id(bus_id, b_id);
        _device_id = device_id;
        set_default();
      };


      /* Begin function to be called in setup: */

      void begin() {
        randomSeed(analogRead(A0));
        delay(random(0, INITIAL_DELAY));
      };


      /* Compose packet in PJON format: */

      uint16_t compose_packet(
        const uint8_t id,
        const uint8_t *b_id,
        char *destination,
        const char *source,
        uint16_t length,
        uint16_t header = NOT_ASSIGNED
      ) const {
        if(header == NOT_ASSIGNED) header = get_header();
        if((header & ACK_REQUEST_BIT) && id == BROADCAST) header &= ~(ACK_REQUEST_BIT);
        if(header > 255) header |= EXTEND_HEADER_BIT;
        if(length > 255) header |= (EXTEND_LENGTH_BIT | CRC_BIT);
        uint16_t new_length = length + packet_overhead(header);
        bool extended_header = header & EXTEND_HEADER_BIT;
        bool extended_length = header & EXTEND_LENGTH_BIT;

        if(new_length > 255 && !extended_length) {
          header |= (EXTEND_LENGTH_BIT | CRC_BIT);
          new_length = (uint16_t)(length + packet_overhead(header));
        }

        if(new_length >= PACKET_MAX_LENGTH) {
          _error(CONTENT_TOO_LONG, new_length);
          return 0;
        }

        destination[0] = id;

        if(extended_header) {
          destination[1] = (uint16_t)header;
          destination[2] = (uint16_t)header >> 8;
        } else destination[1] = header;

        if(extended_length) {
          destination[2 + extended_header] = new_length >> 8;
          destination[3 + extended_header] = new_length & 0xFF;
        } else destination[2 + extended_header] = new_length;

        if(header & MODE_BIT) {
          copy_bus_id((uint8_t*) &destination[3 + extended_header + extended_length], b_id);
          if(header & SENDER_INFO_BIT) {
            copy_bus_id((uint8_t*) &destination[7 + extended_header + extended_length], bus_id);
            destination[11 + extended_header + extended_length] = _device_id;
          }
        } else if(header & SENDER_INFO_BIT)
          destination[3 + extended_header + extended_length] = _device_id;

        memcpy(destination + (new_length - length - (header & CRC_BIT ? 4 : 1)), source, length);

        if(header & CRC_BIT) {
          uint32_t CRC = compute_crc_32((uint8_t *)destination, new_length - 4);
          destination[new_length - 4] = (uint32_t)(CRC) >> 24;
          destination[new_length - 3] = (uint32_t)(CRC) >> 16;
          destination[new_length - 2] = (uint32_t)(CRC) >>  8;
          destination[new_length - 1] = (uint32_t)(CRC);
        } else destination[new_length - 1] = compute_crc_8((uint8_t *)destination, new_length - 1);

        return new_length;
      };


      /* Get the device id, returning a single byte: */

      uint8_t device_id() const {
        return _device_id;
      };


      /* Add a packet to the send list ready to be delivered by the next update() call: */

      uint16_t dispatch(
        uint8_t id,
        const uint8_t *b_id,
        const char *packet,
        uint16_t length,
        uint32_t timing,
        uint16_t header = NOT_ASSIGNED
      ) {
         for(uint8_t i = 0; i < MAX_PACKETS; i++)
          if(packets[i].state == 0) {
            if(!(length = compose_packet(id, b_id, packets[i].content, packet, length, header)))
              return FAIL;
            packets[i].length = length;
            packets[i].state = TO_BE_SENT;
            packets[i].registration = micros();
            packets[i].timing = timing;
            return i;
          }

        _error(PACKETS_BUFFER_FULL, MAX_PACKETS);
        return FAIL;
      };


      /* Return the header byte based on current configuration: */

      uint16_t get_header() const {
        return (_shared ? MODE_BIT : 0) |
               (_sender_info ? SENDER_INFO_BIT : 0) |
               (_acknowledge ? ACK_REQUEST_BIT : 0) |
               (_crc_32 ? CRC_BIT : 0);
      };


      /* Get count of the packets for a device_id:
         Don't pass any parameter to count all packets
         Pass a device id to count all it's related packets */

      uint8_t get_packets_count(uint8_t device_id = NOT_ASSIGNED) const {
        uint8_t packets_count = 0;
        for(uint8_t i = 0; i < MAX_PACKETS; i++) {
          if(packets[i].state == 0) continue;
          if(device_id == NOT_ASSIGNED || packets[i].content[0] == device_id) packets_count++;
        }
        return packets_count;
      };


      /* Calculate the packet's overhead: */

      uint8_t packet_overhead(uint16_t header = NOT_ASSIGNED) const {
        if(header == NOT_ASSIGNED)
          return (_shared ? (_sender_info ? 12 : 7) : (_sender_info ? 4 : 3)) + (_crc_32 ? 4 : 1);
        return (
          (
            (header & MODE_BIT) ?
              (header & SENDER_INFO_BIT    ? 10 : 5) :
              (header & SENDER_INFO_BIT    ?  2 : 1)
          ) + (header & EXTEND_LENGTH_BIT  ?  2 : 1)
            + (header & EXTEND_HEADER_BIT  ?  2 : 1)
            + (header & CRC_BIT            ?  4 : 1)
        );
      };


      /* Fill in a PacketInfo struct by parsing a packet: */

      void parse(const uint8_t *packet, PacketInfo &packet_info) const {
        packet_info.receiver_id = packet[0];
        bool extended_header = packet[1] & EXTEND_HEADER_BIT;
        bool extended_length = packet[1] & EXTEND_LENGTH_BIT;
        packet_info.header = (extended_header) ? packet[2] << 8 | packet[1] : packet[1];
        if((packet_info.header & MODE_BIT) != 0) {
          copy_bus_id(packet_info.receiver_bus_id, packet + 3 + extended_header + extended_length);
          if((packet_info.header & SENDER_INFO_BIT) != 0) {
            copy_bus_id(packet_info.sender_bus_id, packet + 7 + extended_header + extended_length);
            packet_info.sender_id = packet[11 + extended_header + extended_length];
          }
        } else if((packet_info.header & SENDER_INFO_BIT) != 0)
          packet_info.sender_id = packet[3 + extended_header + extended_length];
      };


      uint16_t receive() {
        uint16_t state;
        uint16_t length = PACKET_MAX_LENGTH;
        bool CRC = 0;
        bool extended_header = false;
        bool extended_length = false;
        for(uint16_t i = 0; i < length; i++) {
          data[i] = state = strategy.receive_byte();
          if(state == FAIL) return FAIL;

          if(i == 0)
            if(data[i] != _device_id && data[i] != BROADCAST && !_router)
              return BUSY;

          if(i == 1) {
            if(((data[i] & MODE_BIT) != _shared) && !_router) return BUSY;
            extended_length = data[i] & EXTEND_LENGTH_BIT;
            extended_header = data[i] & EXTEND_HEADER_BIT;
          }

          if((i == (2 + extended_header)) && !extended_length) {
            length = data[i];
            if(length < 5 || length > PACKET_MAX_LENGTH) return FAIL;
          }

          if((i == (3 + extended_header)) && extended_length) {
            length = data[i - 1] << 8 | data[i] & 0xFF;
            if(length < 5 || length > PACKET_MAX_LENGTH) return FAIL;
          }

          if(_shared && (data[1] & MODE_BIT) && !_router)
            if((i > (2 + extended_header + extended_length)))
              if((i < (7 + extended_header + extended_length)))
                if(bus_id[i - 3 - extended_header - extended_length] != data[i])
                  return BUSY;
        }

        if(data[1] & CRC_BIT) {
          uint32_t result = compute_crc_32(data, length - 4);
          for(uint8_t i = 4; i > 0; i--)
            if((uint8_t)(result >> (8 * (i - 1))) != (uint8_t)data[length - i]) {
              CRC = false;
              break;
            } else CRC = true;
        } else CRC = !compute_crc_8(data, length);

        if(data[1] & ACK_REQUEST_BIT && data[0] != BROADCAST && _mode != SIMPLEX && !_router)
          if(
            !_shared || (_shared && (data[1] & MODE_BIT) &&
            bus_id_equality(data + 3 + extended_length + extended_header, bus_id))
          ) strategy.send_response(!CRC ? NAK : ACK);

        if(!CRC) return NAK;
        parse(data, last_packet_info);
        _receiver(
          data + (packet_overhead(data[1]) - (data[1] & CRC_BIT ? 4 : 1)),
          length - packet_overhead(data[1]),
          last_packet_info
        );
        return ACK;
      };


      /* Try to receive a packet repeatedly with a maximum duration: */

      uint16_t receive(uint32_t duration) {
        uint16_t response;
        uint32_t time = micros();
        while((uint32_t)(micros() - time) <= duration) {
          response = receive();
          if(response == ACK)
            return ACK;
        }
        return response;
      };


      /* Remove a packet from the send list: */

      void remove(uint16_t id) {
        packets[id].attempts = 0;
        packets[id].length = 0;
        packets[id].registration = 0;
        packets[id].state = 0;
      };


      /* Remove all packets from the list:
         Don't pass any parameter to delete all packets
         Pass a device id to delete all it's related packets  */

      void remove_all_packets(uint8_t device_id = 0) {
        for(uint8_t i = 0; i < MAX_PACKETS; i++) {
          if(packets[i].state == 0) continue;
          if(!device_id || packets[i].content[0] == device_id) remove(i);
        }
      };


      /* Send a packet to the sender of the last packet received.
         This function is typically called from with the receive
         callback function to deliver a response to a request. */

      uint16_t reply(const char *packet, uint16_t length, uint16_t header = NOT_ASSIGNED) {
        if(last_packet_info.sender_id != BROADCAST)
          return dispatch(
            last_packet_info.sender_id,
            last_packet_info.sender_bus_id,
            packet,
            length,
            0,
            header
          );
        return false;
      };


      /* Insert a packet in the send list:
       The added packet will be sent in the next update() call.
       Using the timing parameter you can set the delay between every
       transmission cyclically sending the packet (use remove() function stop it)

       LOCAL TRANSMISSION -> ISOLATED BUS
       int hi = bus.send(99, "HI!", 3);
       // Send hi once to device 99
       int hi = bus.send_repeatedly(99, "HI!", 3, 1000000);
       // Send HI! to device 99 every second (1.000.000 microseconds)

       NETWORK TRANSMISSION -> SHARED MEDIUM
       uint8_t bus_id[] = {127, 0, 0, 1};
       int hi = bus.send(99, bus_id, "HI!", 3);
       // Send hi once to device 99 on bus id 127.0.0.1
       int hi = bus.send_repeatedly(99, bus_id, "HI!", 3, 1000000);
       // Send HI! to device 99 on bus id 127.0.0.1 every second (1.000.000 microseconds)
       bus.remove(hi); // Stop repeated sending */

      uint16_t send(uint8_t id, const char *string, uint16_t length, uint16_t header = NOT_ASSIGNED) {
        return dispatch(id, bus_id, string, length, 0, header);
      };

      uint16_t send(
        uint8_t id,
        const uint8_t *b_id,
        const char *string,
        uint16_t length,
        uint16_t header = NOT_ASSIGNED
      ) {
        return dispatch(id, b_id, string, length, 0, header);
      };

      /* IMPORTANT: send_repeatedly timing maximum is 4293014170 microseconds or 71.55 minutes */
      uint16_t send_repeatedly(
        uint8_t id,
        const char *string,
        uint16_t length,
        uint32_t timing,
        uint16_t header = NOT_ASSIGNED
      ) {
        return dispatch(id, bus_id, string, length, timing, header);
      };

      /* IMPORTANT: send_repeatedly timing maximum is 4293014170 microseconds or 71.55 minutes */
      uint16_t send_repeatedly(
        uint8_t id,
        const uint8_t *b_id,
        const char *string,
        uint16_t length,
        uint32_t timing,
        uint16_t header = NOT_ASSIGNED
      ) {
        return dispatch(id, b_id, string, length, timing, header);
      };


  /* An Example of how the packet "@" is formatted and sent:

  RECIPIENT ID 12   HEADER 00000110   LENGTH 6          SENDER ID 11      CONTENT 64       CRC
   ________________ _________________ _________________ _________________ ________________ __________________
  |Sync | Byte     |Sync | Byte      |Sync | Byte      |Sync | Byte      |Sync | Byte     |Sync | Byte       |
  |___  |     __   |___  |      _ _  |___  |      _   _|___  |     _   __|___  |  _       |___  |  _      _  |
  |   | |    |  |  |   | |     | | | |   | |     | | | |   | |    | | |  |   | | | |      |   | | | |    | | |
  | 1 |0|0000|11|00| 1 |0|00000|1|1|0| 1 |0|00000|1|0|1| 1 |0|0000|1|0|11| 1 |0|0|1|000000| 1 |0|0|1|0000|1|0|
  |___|_|____|__|__|___|_|_____|_|_|_|___|_|_____|_|_|_|___|_|____|_|_|__|___|_|_|_|______|___|_|_|_|____|_|_|

  A standard packet transmission is a bidirectional communication between
  two devices that can be divided in 3 different phases:

  Channel analysis   Transmission                                                 Response
      _____           ____________________________________________________         _____
     | C-A |         | ID | HEADER | LENGTH |  SENDER ID  | CONTENT | CRC |       | ACK |
  <--|-----|---< >---|----|--------|--------|-------------|---------|-----|--> <--|-----|
     |  0  |         | 12 |00000001|   5    |    ID 11    |   64    |     |       |  6  |
     |_____|         |____|________|________|_____________|_________|_____|       |_____|

  DEFAULT HEADER CONFIGURATION:
  00000110: Acknowledge requested | Sender info included | Local bus

  BUS CONFIGURATION:
  bus.set_acknowledge(true);
  bus.include_sender_info(true);

  Average overhead, average bandwidth availability setup. Can be used only in an isolated
  medium (i.e. isolated wire) and with up to 254 devices with transmission certainty through
  synchronous acknowledge, and sender info to easy reply to packets with the reply() function
  __________________________________________________________________________________________

  A local packet transmission handled in SIMPLEX mode is a monodirectional communication
  between two devices dispatched in a single phase:

     Transmission
      ______________________________________
     | ID | HEADER | LENGTH | CONTENT | CRC |
  >--|----|--------|--------|---------|-----|-->
     | 12 |00000000|   5    |   64    |     |
     |____|________|________|_________|_____|

  HEADER CONFIGURATION:
  00000000: Acknowledge not requested | Sender info not included | Local bus

  BUS CONFIGURATION:
  bus.set_acknowledge(false);
  bus.include_sender_info(false);

  Low overhead, high bandwidth availability setup. Can be used only in an isolated
  medium (i.e. isolated wire) and with up to 254 devices.
  _________________________________________________________________________________________

  A Shared packet transmission example handled in HALF_DUPLEX mode, with acknowledge
  request, including the sender info:

 Channel analysis                         Transmission                                      Response
    _____         __________________________________________________________________         _____
   | C-A |       | ID | HEADER | LENGTH |    BUS ID   | BUS ID | ID | CONTENT | CRC |       | ACK |
 <-|-----|--< >--|----|--------|--------|-------------|--------|----|---------|-----|--> <--|-----|
   |  0  |       | 12 |00000111|   5    |     0001    |  0001  | 11 |   64    |     |       |  6  |
   |_____|       |____|________|________|_____________|________|____|_________|_____|       |_____|
                                        |Receiver info| Sender info |
  HEADER CONFIGURATION:
  00000111: Acknowledge requested | Sender info included | Shared bus

  BUS CONFIGURATION:
  bus.set_acknowledge(true);
  bus.include_sender_info(true);

  High overhead, low bandwidth availability setup. Can be used sharing the medium
  with many other buses with transmission certainty through synchronous acknowledge
  and sender info to easy reply to packets with the reply() function. */

      uint16_t send_packet(const char *string, uint16_t length) {
        if(!string) return FAIL;
        if(_mode != SIMPLEX && !strategy.can_start()) return BUSY;
        strategy.send_string((uint8_t *)string, length);
        if(string[0] == BROADCAST || !_acknowledge || _mode == SIMPLEX) return ACK;
        uint16_t response = strategy.receive_response();
        if(response == ACK || response == NAK || response == FAIL) return response;
        else return BUSY;
      };


      /* Send a packet passing its info as parameters: */

      uint16_t send_packet(uint8_t id, char *string, uint16_t length, uint16_t header = NOT_ASSIGNED) {
        if(!(length = compose_packet(id, bus_id, (char *)data, string, length, header)))
          return FAIL;
        return send_packet((char *)data, length);
      };


      uint16_t send_packet(
        uint8_t id,
        const uint8_t *b_id,
        char *string,
        uint16_t length,
        uint16_t header = NOT_ASSIGNED
      ) {
        if(!(length = compose_packet(id, b_id, (char *)data, string, length, header)))
          return FAIL;
        return send_packet((char *)data, length);
      };


      /* Send a packet without using the send list. It is called send_packet_blocking
         because it tries to transmit a packet multiple times within an internal cycle
         until the packet is delivered, or timing limit is reached. */

      uint16_t send_packet_blocking(
        uint8_t id,
        const uint8_t *b_id,
        const char *string,
        uint16_t length,
        uint16_t header = NOT_ASSIGNED,
        uint32_t timeout = 3000000
      ) {
        if(!(length = compose_packet(id, b_id, (char *)data, string, length, header))) return FAIL;
        uint16_t state = FAIL;
        uint32_t attempts = 0;
        uint32_t time = micros(), start = time;
        while(state != ACK && attempts <= MAX_ATTEMPTS && (uint32_t)(micros() - start) <= timeout) {
          state = send_packet((char*)data, length);
          if(state == ACK) return state;
          attempts++;
          if(state != FAIL) delayMicroseconds(random(0, COLLISION_DELAY));
          while((uint32_t)(micros() - time) < (attempts * attempts * attempts));
          time = micros();
        }
        return state;
      };

      uint16_t send_packet_blocking(
        uint8_t id,
        const char *string,
        uint16_t length,
        uint16_t header = NOT_ASSIGNED
      ) {
        return send_packet_blocking(id, bus_id, string, length, header);
      };


      /* In router mode, the receiver function can ack for selected receiver
         device ids for which the route is known */

      void send_acknowledge() {
        strategy.send_response(ACK);
      };


      /* Configure synchronous acknowledge presence:
         TRUE: Send back synchronous acknowledge when a packet is correctly received
         FALSE: Avoid acknowledge transmission */

      void set_acknowledge(boolean state) {
        _acknowledge = state;
      };


      /* Configure CRC selected for packet checking:
         TRUE:  CRC32
         FALSE: CRC8 */

      void set_crc_32(boolean state) {
        _crc_32 = state;
      };


      /* Set communication mode: */

      void set_communication_mode(uint8_t mode) {
        _mode = mode;
      };


      /* Set bus state default configuration: */

      void set_default() {
        _mode = HALF_DUPLEX;
        if(!bus_id_equality(bus_id, localhost)) _shared = true;
        set_error(dummy_error_handler);
        set_receiver(dummy_receiver_handler);
        for(int i = 0; i < MAX_PACKETS; i++) {
          packets[i].state = 0;
          packets[i].timing = 0;
          packets[i].attempts = 0;
        }
      };


      /* Pass as a parameter a void function you previously defined in your code.
         This will be called when an error in communication occurs

      void error_handler(uint8_t code, uint8_t data) {
        Serial.print(code);
        Serial.print(" ");
        Serial.println(data);
      };

      bus.set_error(error_handler); */

      void set_error(error e) {
        _error = e;
      };


      /* Set the device id, passing a single byte (watch out to id collision): */

      void set_id(uint8_t id) {
        _device_id = id;
      };


      /* Configure sender's information inclusion in the packet.
         TRUE: Includes 1 byte (device id) in local or 5 (bus id + device id) in shared
         FALSE: No inclusion (-1 byte overhead in local / -5 in shared)

         If you don't need the sender info disable the inclusion to reduce overhead and
         higher communication speed. */

      void include_sender_info(bool state) {
        _sender_info = state;
      };


      /* Configure the bus network behaviour.
         TRUE: Enable communication to devices part of other bus ids (on a shared medium).
         FALSE: Isolate communication from external/third-party communication. */

      void set_shared_network(boolean state) {
        _shared = state;
      }


      /* Set if delivered or undeliverable packets are auto deleted:
         TRUE: Automatic deletion
         FALSE: No packet deletion from buffer. Manual packet deletion from buffer is needed.  */

      void set_packet_auto_deletion(boolean state) {
        _auto_delete = state;
      };


      /* Pass as a parameter a void function you previously defined in your code.
         This will be called when a correct message will be received.
         Inside there you can code how to react when data is received.

        void receiver_function(uint8_t *payload, uint16_t length, const PacketInfo &packet_info) {
          for(int i = 0; i < length; i++)
            Serial.print((char)payload[i]);
          Serial.print(" ");
          Serial.println(length);
        };

        bus.set_receiver(receiver_function); */

      void set_receiver(receiver r) {
        _receiver = r;
      };


      /* Configure if device will act as a router:
         FALSE: device receives messages only for its bus and device id
         TRUE:  The receiver function is always called if data is received */

      void set_router(boolean state) {
        _router = state;
      };


      /* Update the state of the send list:
         Check if there are packets to be sent or to be erased if correctly delivered.
         Returns the actual number of packets to be sent. */

      uint8_t update() {
        uint8_t packets_count = 0;
        uint32_t back_off;
        for(uint8_t i = 0; i < MAX_PACKETS; i++) {
          if(packets[i].state == 0) continue;
          packets_count++;
          back_off = packets[i].attempts;
          back_off = back_off * back_off * back_off;
          if((uint32_t)(micros() - packets[i].registration) > packets[i].timing + back_off)
            packets[i].state = send_packet(packets[i].content, packets[i].length);
          else continue;

          if(packets[i].state == ACK) {
            if(!packets[i].timing) {
              if(_auto_delete) {
                remove(i);
                packets_count--;
              }
            } else {
              packets[i].attempts = 0;
              packets[i].registration = micros();
              packets[i].state = TO_BE_SENT;
            } continue;
          }

          if(packets[i].state != FAIL)
            delayMicroseconds(random(0, COLLISION_DELAY));

          packets[i].attempts++;
          if(packets[i].attempts > MAX_ATTEMPTS) {
            _error(CONNECTION_LOST, packets[i].content[0]);
            if(!packets[i].timing) {
              if(_auto_delete) {
                remove(i);
                packets_count--;
              }
            } else {
              packets[i].attempts = 0;
              packets[i].registration = micros();
              packets[i].state = TO_BE_SENT;
            }
          } else packets[i].registration = micros();
        }
        return packets_count;
      };

      uint8_t data[PACKET_MAX_LENGTH];
      PJON_Packet packets[MAX_PACKETS];
      /* A bus id is an array of 4 bytes containing a unique set.
          The default setting is to run a local bus (0.0.0.0), in this
          particular case the obvious bus id is omitted from the packet
          content to reduce overhead. */
      const uint8_t localhost[4] = {0, 0, 0, 0};
      uint8_t bus_id[4] = {0, 0, 0, 0};
      /* Last received packet Metainfo */
      PacketInfo last_packet_info;
    private:
      boolean   _acknowledge = true;
      boolean   _auto_delete = true;
      boolean   _crc_32 = false;
      error     _error;
      uint8_t   _mode;
      receiver  _receiver;
      boolean   _router = false;
      boolean   _sender_info = true;
      boolean   _shared = false;
    protected:
      uint8_t   _device_id;
  };
#endif
