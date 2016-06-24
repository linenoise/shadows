#include "Network.h"

SPIFlash flash(FLASH_SS, FLASH_ID); 

void Network::begin(byte nodeID, byte groupID, byte freq, byte powerLevel) {
	Serial << F("Network. startup.") << endl;

	// EEPROM location for radio settings.
	const byte radioConfigLocation = 42;
	if( nodeID == 255 ) {
		this->myNodeID = EEPROM.read(radioConfigLocation);
		Serial << F("Network. read nodeID from EEPROM=") << this->myNodeID << endl;
	} else {
		Serial << F("Network.  writing nodeID to EEPROM=") << nodeID << endl;
		EEPROM.update(radioConfigLocation, nodeID);
		this->myNodeID = nodeID;
	}
	if( this->myNodeID > 200 ) {
		Serial << F("Network. ERROR no nodeID found in EEPROM!") << endl;
		Serial << F("Enter the node number for this node:") << endl;
		while( ! Serial.available() );
		EEPROM.update(radioConfigLocation, Serial.parseInt());
		return( this->begin(nodeID, groupID, freq, powerLevel) );
	}

	radio.initialize(freq, this->myNodeID, groupID);
	radio.setHighPower(); // using RFM69HW board
	radio.promiscuous(true); // so broadcasts are received
	radio.setPowerLevel(powerLevel);
	
	msg.d[0] = msg.d[1] = msg.d[2] = D_OFFLINE;
	msg.inter[0] = msg.inter[1] = msg.inter[2] = P_OFFLINE;
	msg.range[0] = msg.range[1] = msg.range[2] = P_OFFLINE;
	
	this->lastRxNodeID = 12; // bootstrap to first transceiver
	
	pinMode(LED, OUTPUT);
	digitalWrite(LED, LOW);

	this->setState(M_NORMAL); // default to normal operation
	
	Serial << F("Network. bitrate(bits/s)=55555.") << endl;
	
	// packet contents: http://lowpowerlab.com/blog/2013/06/20/rfm69-library/
	byte packetSizebits = 8*(3+2+4+sizeof(Message)+2);
	
	Serial << F("Network. packet size(bits)=") << packetSizebits << F(" (bytes)=") << packetSizebits/8 << endl;
	
	Serial << F("Network. approximate packet transmission time(ms)=") << (float)packetSizebits / 55.555 << endl;
	
	this->resendInterval = (byte)((float)packetSizebits / 55.555 + 1.0)*2;
	Serial << F("Network. will resend packets every(ms)=") << this->resendInterval << endl;

	Serial << F("Network. startup complete with node number=") << this->myNodeID << endl;
	
}

byte Network::whoAmI() {
	return( this->myNodeID );
}

boolean Network::update() {

	// new traffic?
	if( radio.receiveDone() ) {   
		if( radio.DATALEN==sizeof(Message) ) {
			// read it
			this->msg = *(Message*)radio.DATA;  
			this->lastRxNodeID = radio.SENDERID;
			if( radio.TARGETID == this->myNodeID && radio.ACKRequested() ) {
			      radio.sendACK();
//			      Serial << this->myNodeID << F(": ACK sent to ") << this->lastRxNodeID << endl;
			}
			return( true );
		} else if( radio.TARGETID == this->myNodeID ) {
			// being asked to reprogram ourselves by Gateway?
			CheckForWirelessHEX(this->radio, flash);
		} else if( radio.DATALEN==sizeof(systemState) ) {
			this->setState( *(systemState*)radio.DATA );
		} else if( radio.SENDERID==PROGRAMMER_NODE ) {
			// we need to wait until the airwaves are clear for 5 seconds.
			Serial << F("Network. Programmer traffic.") << endl;
			this->setState( M_PROGRAM );
		}
	}

	// run the reboot commmand in the update cycle
	if( this->currentState == M_REBOOT ) {
		resetUsingWatchdog(true);
	}

	return( false );
}

void Network::printMessage() {
	Serial << F("Network. MSG from ") << this->lastRxNodeID;
	Serial << F("\td 0=") << msg.d[0] << F(" 1=") << msg.d[1] << F(" 2=") << msg.d[2];
	Serial << F("\tp 0=") << msg.inter[0] << F("- ") << msg.range[0] << F("|");
	Serial << F("\tp 1=") << msg.inter[1] << F("- ") << msg.range[1] << F("|");
	Serial << F("\tp 2=") << msg.inter[2] << F("- ") << msg.range[2] << F("|");
	Serial << endl;
}

void Network::setState(systemState state) {
	Serial << F("Network. setting systemState=") << state << endl;
	this->currentState = state;
}
systemState Network::getState() {
	return( this->currentState );
}

byte Network::isNext(byte node, byte maxNode, byte minNode) {
  // skipping modulo arithmetic to minimize instructions
  return( (node+1) >= (maxNode+1) ? minNode : node+1 );
}

byte Network::isPrev(byte node, byte maxNode, byte minNode) {
  return( (node-1) <= (minNode-1) ? maxNode : node-1 );
}

boolean Network::meNext() {
  return( isNext(this->lastRxNodeID, 12, 10) == this->myNodeID );
}

boolean Network::meLast() {
  return( this->lastRxNodeID == this->myNodeID );
}

void Network::send() {
	// put check in to make sure we're not clobbering messages from other transceivers
	this->update();

//	Serial << F("Network. send.") << endl;
	if( this->currentState != M_PROGRAM ) {
//		radio.send(BROADCAST, (const void*)(&this->msg), sizeof(Message));
		static byte nextID = isNext(this->myNodeID, 12, 10);
		while( ! radio.sendWithRetry(nextID, (const void*)(&this->msg), sizeof(Message), 3, this->resendInterval)) {
			this->update();
		}

		this->lastRxNodeID = this->myNodeID;
	}
}

word Network::myDistance() {
  return( this->msg.d[this->myNodeID-20] );
}

word Network::myRange() {
  return( this->msg.range[this->myNodeID-20] );
}

word Network::myIntercept() {
  return( this->msg.inter[this->myNodeID-20] );
}

boolean Network::objectInPlane() {
  return( msg.d[0]<=IN_PLANE || msg.d[1]<=IN_PLANE || msg.d[2]<=IN_PLANE );
}

Network N;
