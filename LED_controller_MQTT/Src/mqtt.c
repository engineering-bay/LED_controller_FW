/*
 * mqtt.c
 *
 *  Created on: 26 сент. 2019 г.
 *      Author: werwolf
 */
#include "mqtt.h"
#include "lwip.h"
#include "api.h"
#include "socket.h"
#include "string.h"


int MqttConnect(int connSocket, char *clientId)
{
  int err = 0;
  //char *clientId = "led_ctrl_020701";
  unsigned char message[29];
  message[0] = MQTT_MESSAGE_TYPE_CONNECT;
  message[1] = 27;
  /* Variable header */
  /* Field: protocol name */
  message[2] = 0;  /* MSB field length*/
  message[3] = 4;  /* LSB field length*/
  message[4] = 'M';
  message[5] = 'Q';
  message[6] = 'T';
  message[7] = 'T';
  /* Field: protocol level */
  message[8] = 4; /* 4 by default*/
  /* Field: connect flags */
  message[9] = MQTT_CONNECTFLAGS_CLEANSESSION;
  /* Field: keep alive */
  message[10] = 0;
  message[11] = 0;
  /* Payload */
  /* Field length */
  message[12] = 0;
  message[13] = 15;
  /* Client ID field */
  sprintf(message + 14, "%s", clientId);

  /* send packet */
  err = send(connSocket, message, sizeof(message), 0);
  return err;
}

int MqttPublish(int connSocket, char *clientId, char *topicName, char *value, int packetId)
{
	int err = 0;
	int index, length;
	//char *clientId = "led_ctrl_020701";
	unsigned char message[50];

	memset(message, 0, 50);
	/* -- Fixed header -- */
	/* Field: message type */
	message[0] = MQTT_MESSAGE_TYPE_PUBLISH;
	/* Field: remaining length - init as 0xff */
	message[1] = 0xff;
	/* -- Variable header -- */
	/* Field: Length MSB & LSB - init as 0xff */
	message[2] = 0xff;
	message[3] = 0xff;
	/* Field: Topic Name */
	sprintf(message + 4, "%s\/%s", clientId, topicName);
	index = strlen(message);
	/* Field: Packet ID */
	/* not used with QoS = 0 */

	/* -- Payload -- */
	sprintf(message + index, "%s", value);
	/* Fill in remaining length field */
	message[1] = strlen(message) - 2;
	/* Fill in variable header length field */
	length = index - 4;
	message[2] = (unsigned char)((length & 0x0000ff00) >> 8);
	message[3] = (unsigned char)(length & 0x000000ff);

	/* send packet */
	err = send(connSocket, message, (int)(message[1] + 2), 0);
	return err;
}

int MqttSubscribe(int connSocket, char *clientId, char *topicName, int packetId)
{
	int err = 0;
	int index, length;
	unsigned char message[50];

	memset(message, 0, 50);
	/* -- Fixed header -- */
	/* Field: message type */
	message[0] = MQTT_MESSAGE_TYPE_SUBSCRIBE | 0x02;
	/* Field: remaining length - init as 0xff */
	message[1] = 0xff;
	/* -- Variable header -- */
	/* Field: PacketId MSB & LSB - init as 0xff */
	message[2] = (unsigned char)((packetId & 0x0000ff00) >> 8);
	message[3] = (unsigned char)(packetId & 0x000000ff);
	/* Payload */
	/* Field: Length MSB & LSB - init as 0xff */
	message[4] = 0xff;
	message[5] = 0xff;
	/* Field: Topic name*/
	sprintf(message + 6, "%s\/%s", clientId, topicName);
	/* Field: Max QoS - set as 0*/
	index = 6 + strlen(clientId) + 1 + strlen(topicName);
	message[index] = 0;
	length = strlen(clientId) + 1 + strlen(topicName);
	message[1] = index - 1;
	message[4] = (unsigned char)((length & 0x0000ff00) >> 8);
	message[5] = (unsigned char)(length & 0x000000ff);

	/* send packet */
	err = send(connSocket, message, (int)(message[1] + 2), 0);
	return err;
}

int MqttParse(unsigned char *buffer, char *clientId, char *cmdTopicName, char *recvCmd, char *recvVal)
{
	int err = -99, rmLength, topicLength, payloadLength, topicStartIndex;
	switch(buffer[0])
	{
	case MQTT_MESSAGE_TYPE_CONNACK:
		if(buffer[1] == 2)
		{
			if(buffer[3] == 0) err = 100;
			else err = buffer[3];
			//connState = CONNSTATE_MQTT_CONNECTED;
		}
		else err = -1;
		break;
	case MQTT_MESSAGE_TYPE_PUBLISH:
		rmLength = buffer[1];
		topicLength = (int)buffer[3] + (int)(buffer[2] << 8);
		if(topicLength > rmLength - 3) err = -2;
		else
		{
			if(strncmp((char *)(buffer + 4), clientId, strlen(clientId)) == 0)
			{
				if(strncmp((char *)(buffer + 4 + strlen(clientId) + 1), cmdTopicName, strlen(cmdTopicName)) == 0)
				{
					payloadLength = rmLength - topicLength - 2;
					topicStartIndex = 4 + strlen(clientId) + 1 + strlen(cmdTopicName) + 1;
					if((topicLength - topicStartIndex) < 1) err = -5;
					else
					{
						strncpy(recvCmd, (char *)(buffer + topicStartIndex), (rmLength + 2 - topicStartIndex - payloadLength));
						strncpy(recvVal, (char *)(buffer + rmLength + 2 - payloadLength), payloadLength);
						err = 101;
					}
				}
				else err = -4;
			}
			else err = -3;
		}
		break;
	}
	return err;
}
