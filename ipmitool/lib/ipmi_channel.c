/*
 * Copyright (c) 2003 Sun Microsystems, Inc.  All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistribution of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * Redistribution in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of Sun Microsystems, Inc. or the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * 
 * This software is provided "AS IS," without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * SUN MICROSYSTEMS, INC. ("SUN") AND ITS LICENSORS SHALL NOT BE LIABLE
 * FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
 * SUN OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA,
 * OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR
 * PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF
 * LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
 * EVEN IF SUN HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * 
 * You acknowledge that this software is not designed or intended for use
 * in the design, construction, operation or maintenance of any nuclear
 * facility.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi_channel.h>



const struct valstr ipmi_authtype_vals[] = {
			{ IPMI_1_5_AUTH_TYPE_BIT_NONE,     "NONE" },
			{ IPMI_1_5_AUTH_TYPE_BIT_MD2,      "MD2" },
			{ IPMI_1_5_AUTH_TYPE_BIT_MD5,      "MD5" },
			{ IPMI_1_5_AUTH_TYPE_BIT_PASSWORD, "PASSWORD" },
			{ IPMI_1_5_AUTH_TYPE_BIT_OEM,      "OEM" },
			{ 0,                               NULL },
		};



static const struct valstr ipmi_channel_protocol_vals[] = {
			{ 0x00, "reserved" },
			{ 0x01, "IPMB-1.0" },
			{ 0x02, "ICMB-1.0" },
			{ 0x03, "reserved" },
			{ 0x04, "IPMI-SMBus" },
			{ 0x05, "KCS" },
			{ 0x06, "SMIC" },
			{ 0x07, "BT-10" },
			{ 0x08, "BT-15" },
			{ 0x09, "TMode" },
			{ 0x1c, "OEM 1" },
			{ 0x1d, "OEM 2" },
			{ 0x1e, "OEM 3" },
			{ 0x1f, "OEM 4" },
			{ 0x00, NULL },
		};


static const struct valstr ipmi_channel_medium_vals[] = {
			{ 0x00, "reserved" },
			{ 0x01, "IPMB (I2C)" },
			{ 0x02, "ICMB v1.0" },
			{ 0x03, "ICMB v0.9" },
			{ 0x04, "802.3 LAN" },
			{ 0x05, "Serial/Modem" },
			{ 0x06, "Other LAN" },
			{ 0x07, "PCI SMBus" },
			{ 0x08, "SMBus v1.0/v1.1" },
			{ 0x09, "SMBus v2.0" },
			{ 0x0a, "USB 1.x" },
			{ 0x0b, "USB 2.x" },
			{ 0x0c, "System Interface" },
			{ 0x00, NULL },
		};



/**
 * impi_1_5_authtypes
 *
 * Create a string describing the supported authentication types as 
 * specificed by the parameter n
 */
const char * impi_1_5_authtypes(unsigned char n)
{
	unsigned int i;
	static char supportedTypes[128];

	bzero(supportedTypes, 128);

	i = 0;
	while (ipmi_authtype_vals[i].val)
	{
		if (n & ipmi_authtype_vals[i].val)
		{
			strcat(supportedTypes, ipmi_authtype_vals[i].str);
			strcat(supportedTypes, " ");
		}

		++i;
	}

	return supportedTypes;
}



/**
 * ipmi_get_channel_auth_cap
 *
 * Wrapper around the Get Channel Authentication Capabilities command
 */
void ipmi_get_channel_auth_cap(struct ipmi_intf * intf,
							   unsigned char channel,
							   unsigned char priv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct get_channel_auth_cap_rsp auth_cap;


	unsigned char msg_data[2];
	msg_data[0] = channel | 0x80; // Ask for IPMI v2 data as well
	msg_data[1] = priv;

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_APP;            // 0x06
	req.msg.cmd      = IPMI_GET_CHANNEL_AUTH_CAP; // 0x38
	req.msg.data     = msg_data;
	req.msg.data_len = 2;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp || rsp->ccode)
	{
		/*
		 * It's very possible that this failed because we asked for IPMI v2 data
		 * Ask again, without requesting IPMI v2 data
		 */
		msg_data[0] &= 0x7F;
		
		rsp = intf->sendrecv(intf, &req);
		if (!rsp || rsp->ccode) {
			printf("Error:%x Get Channel Authentication Capabilities Command (0x%x)\n",
				   rsp ? rsp->ccode : 0, channel);
			return;
		}
	}

	memcpy(&auth_cap, rsp->data, sizeof(struct get_channel_auth_cap_rsp));


	printf("Channel number             : %d\n",
		   auth_cap.channel_number);
	printf("IPMI v1.5  auth types      : %s\n",
		   impi_1_5_authtypes(auth_cap.enabled_auth_types));

	if (auth_cap.v20_data_available)
		printf("KG status                  : %s\n",
			   (auth_cap.kg_status) ? "non-zero" : "default (all zeroes)");

	printf("Per message authentication : %sabled\n",
		   (auth_cap.per_message_auth) ? "en" : "dis");
	printf("User level authentication  : %sabled\n",
		   (auth_cap.user_level_auth) ? "en" : "dis");

	printf("Non-null user names exist  : %s\n",
		   (auth_cap.non_null_usernames) ? "yes" : "no");
	printf("Null user names exist      : %s\n",
		   (auth_cap.null_usernames) ? "yes" : "no");
	printf("Anonymous login enabled    : %s\n",
		   (auth_cap.anon_login_enabled) ? "yes" : "no");

	if (auth_cap.v20_data_available)
	{
		printf("Channel supports IPMI v1.5 : %s\n",
			   (auth_cap.ipmiv15_support) ? "yes" : "no");
		printf("Channel supports IPMI v2.0 : %s\n",
			   (auth_cap.ipmiv20_support) ? "yes" : "no");
	}

	/*
	 * If there is support for an OEM authentication type, there is some
	 * information.
	 */
	if (auth_cap.enabled_auth_types & IPMI_1_5_AUTH_TYPE_BIT_OEM)
	{
		printf("IANA Number for OEM        : %d\n",
			   auth_cap.oem_id[0]      | 
			   auth_cap.oem_id[1] << 8 | 
			   auth_cap.oem_id[2] << 16);
		printf("OEM Auxiliary Data         : 0x%x\n",
			   auth_cap.oem_aux_data);
	}
}



/**
 * ipmi_get_channel_info
 *
 * Wrapper around the Get Channel Info command
 */
void
ipmi_get_channel_info(struct ipmi_intf * intf, unsigned char channel)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char rqdata[2];
	struct get_channel_info_rsp   channel_info;
	struct get_channel_access_rsp channel_access;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;        // 0x06
	req.msg.cmd   = IPMI_GET_CHANNEL_INFO; // 0x42
	req.msg.data = &channel;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp || rsp->ccode) {
		printf("Error:%x Get Channel Info Command (0x%x)\n",
			   rsp ? rsp->ccode : 0, channel);
		return;
	}


	memcpy(&channel_info, rsp->data, sizeof(struct get_channel_info_rsp));


	printf("Channel 0x%x info:\n", channel_info.channel_number);

	printf("  Channel Medium Type   : %s\n",
		   val2str(channel_info.channel_medium, ipmi_channel_medium_vals));

	printf("  Channel Protocol Type : %s\n",
		   val2str(channel_info.channel_protocol, ipmi_channel_protocol_vals));

	printf("  Session Support       : ");
	switch (channel_info.session_support) {
		case 0x00:
			printf("session-less\n");
			break;
		case 0x40:
			printf("single-session\n");
			break;
		case 0x80:
			printf("multi-session\n");
			break;
		case 0xc0:
		default:
			printf("session-based\n");
			break;
	}

	printf("  Active Session Count  : %d\n",
		   channel_info.active_sessions);

	printf("  Protocol Vendor ID    : %d\n",
		   channel_info.vendor_id[0]      |
		   channel_info.vendor_id[1] << 8 |
		   channel_info.vendor_id[2] << 16);



	memset(&req, 0, sizeof(req));
	rqdata[0] = channel & 0xf;

	/* get volatile settings */

	rqdata[1] = 0x80; /* 0x80=active */
	req.msg.netfn = IPMI_NETFN_APP;          // 0x06
	req.msg.cmd   = IPMI_GET_CHANNEL_ACCESS; // 0x41
	req.msg.data = rqdata;
	req.msg.data_len = 2;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		return;
	}

	memcpy(&channel_access, rsp->data, sizeof(struct get_channel_access_rsp));


	printf("  Volatile(active) Settings\n");
	printf("    Alerting            : %sabled\n",
		   (channel_access.alerting) ? "dis" : "en");
	printf("    Per-message Auth    : %sabled\n",
		   (channel_access.per_message_auth) ? "dis" : "en");
	printf("    User Level Auth     : %sabled\n",
		   (channel_access.user_level_auth) ? "dis" : "en");

	printf("    Access Mode         : ");
	switch (channel_access.access_mode) {
		case 0:
			printf("disabled\n");
			break;
		case 1:
			printf("pre-boot only\n");
			break;
		case 2:
			printf("always available\n");
			break;
		case 3:
			printf("shared\n");
			break;
		default:
			printf("unknown\n");
			break;
	}

	/* get non-volatile settings */

	rqdata[1] = 0x40; /* 0x40=non-volatile */
	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		return;
	}

	memcpy(&channel_access, rsp->data, sizeof(struct get_channel_access_rsp));

	printf("  Non-Volatile Settings\n");
	printf("    Alerting            : %sabled\n",
		   (channel_access.alerting) ? "dis" : "en");
	printf("    Per-message Auth    : %sabled\n",
		   (channel_access.per_message_auth) ? "dis" : "en");
	printf("    User Level Auth     : %sabled\n",
		   (channel_access.user_level_auth) ? "dis" : "en");

	printf("    Access Mode         : ");
	switch (channel_access.access_mode) {
		case 0:
			printf("disabled\n");
			break;
		case 1:
			printf("pre-boot only\n");
			break;
		case 2:
			printf("always available\n");
			break;
		case 3:
			printf("shared\n");
			break;
		default:
			printf("unknown\n");
			break;
	}

}



void
printf_channel_usage()
{
	printf("Channel Commands: authcap <channel number> <max priv>\n");
	printf("                  info    [channel number]\n");
}



int
ipmi_channel_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int retval = 0;

	if (!argc || !strncmp(argv[0], "help", 4))
		printf_channel_usage();

	else if (!strncmp(argv[0], "authcap", 7))
	{
		if (argc != 3)
			printf_channel_usage();
		else
			ipmi_get_channel_auth_cap(intf,
									  (unsigned char)strtol(argv[1], NULL, 0),
									  (unsigned char)strtol(argv[2], NULL, 0));
	}
	else if (!strncmp(argv[0], "info", 4))
	{
		if (argc > 2)
			printf_channel_usage();
		else
		{
			if (argc == 2)
				ipmi_get_channel_info(intf, (unsigned char)strtol(argv[1], NULL, 0));
			else
				ipmi_get_channel_info(intf, 0xe);
		}
	}
	else
	{
		printf("Invalid CHANNEL command: %s\n", argv[0]);
		printf_channel_usage();
		retval = 1;
	}

	return retval;
}

