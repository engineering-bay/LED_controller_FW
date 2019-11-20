/*
 * mqtt.h
 *
 *  Created on: 26 сент. 2019 г.
 *      Author: werwolf
 */

#ifndef MQTT_H_
#define MQTT_H_

/* MQTT connection states*/
#define MQTT_CONN_NOTCONN		0x00
#define MQTT_CONN_CONNECTED		0x01
/* MQTT message types*/
#define MQTT_MESSAGE_TYPE_RESERVED		0x00
#define MQTT_MESSAGE_TYPE_CONNECT		0x10
#define MQTT_MESSAGE_TYPE_CONNACK		0x20
#define MQTT_MESSAGE_TYPE_PUBLISH		0x30
#define MQTT_MESSAGE_TYPE_PUBACK		0x40
#define MQTT_MESSAGE_TYPE_PUBREC		0x50
#define MQTT_MESSAGE_TYPE_PUBREL		0x60
#define MQTT_MESSAGE_TYPE_PUBCOMP		0x70
#define MQTT_MESSAGE_TYPE_SUBSCRIBE		0x80
#define MQTT_MESSAGE_TYPE_SUBACK		0x90
#define MQTT_MESSAGE_TYPE_UNSUBSCRIBE	0xA0
#define MQTT_MESSAGE_TYPE_UNSUBACK		0xB0
#define MQTT_MESSAGE_TYPE_PINGREQ		0xC0
#define MQTT_MESSAGE_TYPE_PINGRESP		0xD0
#define MQTT_MESSAGE_TYPE_DISCONNECT	0xE0
#define MQTT_MESSAGE_TYPE_RESERVED2		0xF0

#define MQTT_MESSAGE_DUP_FLAG			0x08

#define MQTT_MESSAGE_QOS_LEVEL0			0x00
#define MQTT_MESSAGE_QOS_LEVEL1			0x02
#define MQTT_MESSAGE_QOS_LEVEL2			0x04
#define MQTT_MESSAGE_QOS_LEVEL3			0x06

#define MQTT_MESSAGE_RETAIN_FLAG		0x01

/* MQTT connect flags*/
#define MQTT_CONNECTFLAGS_CLEANSESSION	0x02
#define MQTT_CONNECTFLAGS_WILLFLAG		0x04
#define MQTT_CONNECTFLAGS_WILLQOS1		0x08
#define MQTT_CONNECTFLAGS_WILLQOS2		0x10
#define MQTT_CONNECTFLAGS_WILLRETAIN	0x20
#define MQTT_CONNECTFLAGS_PASSWORDFLAG	0x40
#define MQTT_CONNECTFLAGS_USERNAMEFLAG	0x80


int MqttConnect(int connSocket, char *clientId);
int MqttPublish(int connSocket, char *clientId, char *topicName, char *value, int packetId);
int MqttSubscribe(int connSocket, char *clientId, char *topicName, int packetId);
int MqttParse(unsigned char *buffer, char *clientId, char *cmdTopicName, char *recvCmd, char *recvVal);

#endif /* MQTT_H_ */
