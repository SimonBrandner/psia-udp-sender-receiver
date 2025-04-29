# PSIA UDP File Transfer Protocol Spec

In this document, we specify the protocol that we implement in our PSIA semestral work. For simplicity this protocol is not versioned.

## Packet

Each packet has the following form (see following sections for details):

- Packet type (8 bits) -- info about the packet type
- Transmission ID (32 bits) -- a randomly (for uniqueness) generated number
- Packet content (the rest of the packet) -- content of the packet
- CRC (32 bits) -- the CRC-32 of the packet

### Sender packet types

Here we list packet types that will be sent by the file sender.

#### Transmission Start

- Packet type -- `0x00`
- Packet content
  - Transmission length (32 bits) -- a number indicating the number of data packets that will be sent
  - File name (the rest of the packet) -- chars of the filename that ends with **\0** \*e.g. `"sample.png\0"`

#### Transmission Data

- Packet type -- `0x01`
- Packet content
  - Data packet index (32 bits) -- a number in range [0, transmission length - 1] indicating the data packet index
  - Data (the rest of the packet) -- the data being sent

#### Transmission End

- Packet type -- `0x02`
- Packet content
  - File size (32 bits) -- the file size in bytes
  - Hash (256 bits) -- a SHA-256 hash of only the file content

### Receiver packet types

Here we list packet types that will be sent by the file receiver.

#### Transmission end response

- Packet type -- `0x03`
- Packet content
  - Status (8 bit) -- boolean indicating whether or not the hash and file size match on the receiver side

> If the hash **does** match, receiver will close the socket in 10 seconds after that moment.

> If the hash **does not** match, the process (communication) will start once over from packet `0x00`

#### Acknowledgement

- Packet type -- `0x04`
- Packet content
  - Packet type (8 bits) -- the type of the packet to which we are reacting
  - Status (8 bits) -- boolean indicating whether or not the received packet has the correct CRC
  - Index (32 bits) -- index of the corrupted data transmission packet (only present if packet type in packet content is `0x01`)

## Alternatives and other notes

### Type prefix bit to indicate sender vs receiver

The first bit of the packet type field could be designated to identifying whether this packet was sent by a sender or a receiver. This could have potential benefit if we wanted to use one port and have multiple sender/receivers.
