# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]


## Release v1.31.32 - 2024-05-03(14:50:21 +0000)

### Other

- XMPP support - 1st implementation

## Release v1.31.31 - 2024-04-18(15:09:17 +0000)

### Fixes

- [tr069-manager] Make it compatible with openwrt23.05 toolchain

## Release v1.31.30 - 2024-04-11(15:17:11 +0000)

### Other

- A Manageable device can't be added after reboot

## Release v1.31.29 - 2024-04-10(15:54:56 +0000)

### Other

- Upload RPC to upload log not working as expected

## Release v1.31.28 - 2024-04-10(11:54:20 +0000)

### Other

- Add ScheduleDownload RPC

## Release v1.31.27 - 2024-04-10(11:45:03 +0000)

### Other

- Upload RPC to upload log not working as expected

## Release v1.31.26 - 2024-04-10(10:10:42 +0000)

### Changes

- Make amxb timeouts configurable

## Release v1.31.25 - 2024-04-05(13:10:42 +0000)

### Fixes

- firmware upgrade isn't working anymore

## Release v1.31.24 - 2024-04-03(15:35:18 +0000)

### Fixes

- Issue: HOP 6211 DUT doesn't send '8 Diagnostics Complete' to the ACS

## Release v1.31.23 - 2024-03-28(16:04:54 +0000)

### Other

- [tr069-manager] Implement defaults.d directory

## Release v1.31.22 - 2024-03-26(12:09:30 +0000)

### Other

- [TR069] InformParameter wildcard support

## Release v1.31.21 - 2024-03-22(08:27:34 +0000)

### Other

- [TR-069] Ability to configure default subscriptions under...

## Release v1.31.20 - 2024-03-21(15:00:32 +0000)

### Other

- TR69 persistence at reboot is broken

## Release v1.31.19 - 2024-03-20(09:20:55 +0000)

### Other

- [TR069] InformParameter wildcard support

## Release v1.31.18 - 2024-03-19(17:52:34 +0000)

### Other

- [TR069] InformParameter wildcard support

## Release v1.31.17 - 2024-03-19(17:29:13 +0000)

### Other

- [Reboot Reason][tr-069] Provide the correct Reboot reason

## Release v1.31.16 - 2024-03-19(09:23:05 +0000)

### Other

- [Tr181-Device] Move cwmp_plugin from ProxyManager to Tr181-Device extension

## Release v1.31.15 - 2024-03-18(14:41:05 +0000)

### Other

- Rework TR-181 interface stacks indexing number

## Release v1.31.14 - 2024-03-16(06:19:31 +0000)

### Other

- [TR069] missing parameters in the Inform (PERIODIC)

## Release v1.31.13 - 2024-03-15(15:57:53 +0000)

### Other

- [TR069] missing parameters in the Inform (PERIODIC)

## Release v1.31.12 - 2024-03-12(08:32:43 +0000)

### Other

- Update HeartbeatPolicy.Enable default value

## Release v1.31.11 - 2024-03-11(13:52:15 +0000)

### Other

- Update HeartbeatPolicy.ReportingInterval default value

## Release v1.31.10 - 2024-03-08(14:16:50 +0000)

### Other

- [TR069] Invalid fault code received when setting an "Invalid parameter type" and "Invalid parameter value"

## Release v1.31.9 - 2024-03-05(13:21:56 +0000)

### Other

- [Device] Missing notifications when a plugin restarts

## Release v1.31.8 - 2024-02-29(10:18:33 +0000)

### Other

- Add support of Heartbeat Inform

## Release v1.31.7 - 2024-02-26(08:34:42 +0000)

### Other

- Repetitive CWMPD crash

## Release v1.31.6 - 2024-02-26(08:24:41 +0000)

### Other

- Make all warnings into errors and fix them

## Release v1.31.5 - 2024-02-16(14:54:30 +0000)

### Other

- [tr069-manager][tests] Add amxut lib

## Release v1.31.4 - 2024-02-16(14:05:46 +0000)

### Other

- Cannot SPV the Description parameter under Device.NAT.PortMapping.1.

## Release v1.31.3 - 2024-02-07(13:56:54 +0000)

### Other

- Firmware upgrade not working with HTTPS

## Release v1.31.2 - 2024-01-23(15:27:13 +0000)

### Other

- the hgw sends "7 TRANSFER COMPLETE" before reboot

## Release v1.31.1 - 2024-01-19(17:14:45 +0000)

### Other

- ACS Session lasts 30s more than expected

## Release v1.31.0 - 2024-01-12(16:32:55 +0000)

### New

- Annex F add repeater mode

## Release v1.30.8 - 2024-01-11(14:17:44 +0000)

### Other

- ManagementServer.State.LastSession is not updated correctly after upgrade

## Release v1.30.7 - 2024-01-09(13:36:02 +0000)

### Fixes

- Issue : HOP-5294 Performance issue on getParameterValues

## Release v1.30.6 - 2023-12-15(16:41:35 +0000)

### Other

- ACS session timeout with Nokia certification platform

## Release v1.30.5 - 2023-12-12(16:10:45 +0000)

### Fixes

- Fw rule for conn_req recreated multiple times on wan reset

## Release v1.30.4 - 2023-12-12(13:07:53 +0000)

### Other

- [TR-69] cwmpd add client cert and private key

## Release v1.30.3 - 2023-12-11(16:11:30 +0000)

### Other

- [TR-69] cwmpd add client cert and private key

## Release v1.30.2 - 2023-12-06(15:17:05 +0000)

### Other

- [TR-069] Connection Request Failed

## Release v1.30.1 - 2023-12-04(14:29:21 +0000)

### Other

- The DUT no longer listens to connection requests on the port after updating the ACS URL

## Release v1.30.0 - 2023-12-01(15:53:02 +0000)

### Security

- Admin user can update cwmp configuration

## Release v1.29.4 - 2023-11-30(10:22:54 +0000)

## Release v1.29.3 - 2023-11-29(16:56:18 +0000)

### Other

- - Definition of Upgrade Persistent CWMPD Configuration

## Release v1.29.2 - 2023-11-22(12:04:27 +0000)

### Other

- ACS Session not created when ManagementServer.URL parameter is updated

## Release v1.29.1 - 2023-11-20(13:06:11 +0000)

### Other

- [prplos][Mxl-osp][tr69-manager] All tr69 cdrouter tests are failing

## Release v1.29.0 - 2023-10-24(08:33:43 +0000)

### Other

- All applications using sahtrace logs should use default log levels

## Release v1.28.0 - 2023-10-24(08:23:18 +0000)

### Other

- [Coverage] Remove SAHTRACE defines in order to increase branching coverage

## Release v1.27.2 - 2023-10-12(16:58:32 +0000)

### Other

- [VZ] TR-069 failed to retrieve parametershttps

## Release v1.27.1 - 2023-09-06(10:07:48 +0000)

### Fixes

- Wrong behavior GPV with invalid path

## Release v1.27.0 - 2023-08-25(13:24:27 +0000)

### New

- fw upgrade rewire to DeviceInfo upgrade API

## Release v1.26.2 - 2023-07-31(12:26:10 +0000)

### Fixes

- fix unit tests in tr069-manager

## Release v1.26.1 - 2023-07-31(05:38:39 +0000)

### Fixes

- cwmp is not starting

## Release v1.26.0 - 2023-06-21(16:44:53 +0000)

### New

- Use acls permissions for cwmp user

## Release v1.25.0 - 2023-06-21(13:08:33 +0000)

### New

- ManagementServer DM add specific params

### Fixes

- Split cwmp_plugin/cwmpd config

## Release v1.24.0 - 2023-06-06(09:14:24 +0000)

### New

- InstanceAlias mode add support for mixed paths

## Release v1.23.2 - 2023-06-05(08:54:35 +0000)

## Release v1.23.1 - 2023-05-29(13:22:06 +0000)

### Fixes

- ManagementServer.Subscription not BBF compliant

## Release v1.23.0 - 2023-05-25(14:53:48 +0000)

### New

-  Add support for ManageableDevices

## Release v1.22.4 - 2023-04-26(15:30:33 +0000)

### Fixes

- [odl] Remove deprecated odl keywords

## Release v1.22.3 - 2023-04-20(13:14:16 +0000)

### Fixes

- service protocol should be integers

## Release v1.22.2 - 2023-04-17(11:24:49 +0000)

### Fixes

- [odl]Remove deprecated odl keywords

## Release v1.22.1 - 2023-03-30(10:06:34 +0000)

### Fixes

- fix fw upgrade fail
- Box does not upload the backup file
- open firewall port according to the IP version of ConRequestURL
- no errors generated for non existing objects
- Device.ManagementServer is missing in version 0.5.1

## Release v1.22.0 - 2023-03-23(15:23:42 +0000)

### Other

- Use sah_libc-ares instead of opensource_c-ares

## Release v1.21.3 - 2023-03-15(11:16:29 +0000)

### Changes

- [Config] enable configurable coredump generation

## Release v1.21.2 - 2023-02-21(09:31:43 +0000)

### Other

- Implementation of InstanceMode="InstanceAlias" [New]

## Release v1.21.1 - 2023-02-15(14:14:21 +0000)

### Fixes

- cd router tests are completely failing for tr-069

## Release v1.21.0 - 2023-02-02(09:48:55 +0000)

### New

- Add support for ipv6

## Release v1.20.0 - 2023-01-05(11:53:28 +0000)

### New

- [import-dbg] Disable import-dbg by default for all amxrt plugin

## Release v1.19.2 - 2022-12-21(14:31:41 +0000)

### Fixes

- Download file is not saved under tmp

## Release v1.19.1 - 2022-12-13(15:36:31 +0000)

### Fixes

- Remove the m4 extension of the odl definition file

## Release v1.19.0 - 2022-12-09(11:45:46 +0000)

### New

- Implement Download/Upload/GetQueuedTransfers

## Release v1.18.2 - 2022-11-22(13:35:52 +0000)

### Fixes

- crash when performing a GPN on Device.NonExistent parameter

## Release v1.18.1 - 2022-11-22(09:02:29 +0000)

### Fixes

- Crash cwmpd

## Release v1.18.0 - 2022-11-16(12:56:03 +0000)

### Fixes

- Cwmpd process leaking when retrying to reach ACS

## Release v1.17.3 - 2022-10-07(16:19:26 +0000)

### Fixes

- InterfaceStackNumberOfEntries wrong type

## Release v1.17.2 - 2022-10-04(08:19:26 +0000)

### Fixes

- Add some missing parameters to cwmp_plugin odl

## Release v1.17.1 - 2022-09-20(15:46:21 +0000)

### Fixes

- Fix cwmp free

## Release v1.17.0 - 2022-09-13(13:13:22 +0000)

### New

- The amx TR069 client must be adapted to work with ACL's

## Release v1.16.24 - 2022-09-13(13:05:27 +0000)

### Fixes

- ubus reported datetime as string type

## Release v1.16.23 - 2022-08-23(11:19:24 +0000)

### Fixes

- Fix cwmpd crash when excute setParameterValues with object not found

## Release v1.16.22 - 2022-08-23(11:14:30 +0000)

### Fixes

- Fix Digest auth fails as the box send an empty cnonce

## Release v1.16.21 - 2022-08-11(10:22:00 +0000)

### Fixes

- Issue : HOP-1466 Fix Digest Authentication and refactoring code

## Release v1.16.20 - 2022-08-11(09:59:25 +0000)

### Other

- It is not possible to contact the box using the ConnReqURL,...

## Release v1.16.19 - 2022-08-03(09:02:06 +0000)

### Other

- Fix wrong error code returned when making a SetParameterValue...

## Release v1.16.18 - 2022-07-29(12:53:03 +0000)

### Fixes

- Check if wan is not connected, no starting cwmpd server

## Release v1.16.17 - 2022-07-12(16:17:35 +0000)

### Fixes

- Fix PeriodicInformTime format
- GPN/GPA fail with parameter search path

## Release v1.16.16 - 2022-07-04(15:49:47 +0000)

### Fixes

- missing "M Reboot" in inform msg after reboot
- Remove duplicated env script from tr069-manager

## Release v1.16.15 - 2022-07-04(15:28:27 +0000)

### Fixes

- warning on some parameter types

## Release v1.16.14 - 2022-07-04(15:24:13 +0000)

### Fixes

- Firewall service should be required by cwmpd
- TCP connection is not persistent

## Release v1.16.13 - 2022-06-20(11:53:44 +0000)

### Fixes

- Fix Set session cookies

## Release v1.16.12 - 2022-06-20(11:47:24 +0000)

### Fixes

- GPV RPC stop on a non-tr181 components

## Release v1.16.11 - 2022-06-10(15:05:30 +0000)

### Fixes

- Empty POST must not contain a SOAPAction and content-Type headers

## Release v1.16.10 - 2022-06-09(11:27:02 +0000)

### Other

- [Gitlab CI][Unit tests][valgrind] Pipeline doesn't stop when memory leaks are detected

## Release v1.16.9 - 2022-06-08(14:59:59 +0000)

### Fixes

- Fix saving attributes cache and schedulerinform
- Fix memleaks on cwmpd

## Release v1.16.8 - 2022-05-18(07:48:42 +0000)

### Fixes

- libwebsockets crash after the first server reply in a multi client context

## Release v1.16.7 - 2022-05-03(13:02:31 +0000)

### Fixes

- Fix cwmpd client append handshake headers

## Release v1.16.6 - 2022-04-29(14:50:33 +0000)

### Fixes

- failed to generate auth headers
- SIGABRT when timer is started twice before it is expire

## Release v1.16.5 - 2022-04-13(15:35:55 +0000)

### Fixes

- Fix cwmpd crash when excuting DHCP renew

## Release v1.16.4 - 2022-04-11(15:35:03 +0000)

### Fixes

- Gitlab pipeline: complexity-check warnings

## Release v1.16.3 - 2022-04-06(13:48:25 +0000)

### Fixes

- Fix SIGFPE when DNS resolution fail

## Release v1.16.2 - 2022-03-30(08:43:37 +0000)

### Fixes

- Notify cwmpd when connectionRequestURL is modified

## Release v1.16.1 - 2022-03-29(14:58:53 +0000)

### Fixes

- Shutdown Notif interface when exit, Fix errors

## Release v1.16.0 - 2022-03-29(11:27:46 +0000)

### New

- add support for digest authentication

## Release v1.15.0 - 2022-03-28(12:59:54 +0000)

### New

- rework cwmpd configs

## Release v1.14.3 - 2022-03-24(11:19:52 +0000)

### Changes

- [GetDebugInformation] Add data model debuginfo in component services

## Release v1.14.2 - 2022-03-23(13:51:23 +0000)

### Fixes

- send notif when connrequrl is changed

## Release v1.14.1 - 2022-03-18(15:20:33 +0000)

### Changes

- Enable core dumps by default

## Release v1.14.0 - 2022-03-17(13:32:53 +0000)

### New

- Open the port for connection request

### Fixes

- Fix cwmpd crash if DHCP Client instance goes down

## Release v1.13.1 - 2022-03-16(12:05:44 +0000)

### Other

- support for a http stateless mode

## Release v1.13.0 - 2022-03-10(09:53:14 +0000)

### New

- Use netmodel to get wan interface info

## Release v1.12.2 - 2022-03-08(17:26:57 +0000)

### Changes

- Make DNS fully async

## Release v1.12.1 - 2022-02-17(17:58:25 +0000)

### Fixes

- issue: HOP-1028 connection error when host is set

## Release v1.12.0 - 2022-02-17(15:50:57 +0000)

### New

- - Add subscription to data model

## Release v1.11.2 - 2022-02-16(17:49:37 +0000)

### Other

- Avoid casting const char* to char*

## Release v1.11.1 - 2022-02-15(09:03:04 +0000)

### Fixes

- It should be possible to setup some odl default values from environment

## Release v1.11.0 - 2022-02-10(16:45:29 +0000)

### New

- Add unit tests

## Release v1.10.0 - 2022-02-09(16:06:33 +0000)

### New

- Add ssl support for the cwmp_client

### Fixes

- Issue : PCF-546 Properly generate coverage report

## Release v1.9.4 - 2022-01-27(08:05:37 +0000)

### Fixes

- Cleanup and fix some cwmpd issues

## Release v1.9.3 - 2022-01-21(19:18:17 +0000)

### Fixes

- Expose only the standard parameters to ACS

## Release v1.9.2 - 2022-01-14(08:54:48 +0000)

### Fixes

- Set mod_sahtrace as an optional include in odl file

## Release v1.9.1 - 2022-01-13(11:16:54 +0000)

### Fixes

- Clean up data model from non standard parameters

## Release v1.9.0 - 2022-01-13(09:59:51 +0000)

### New

- Find and update wan Interface and IP address

## Release v1.8.2 - 2022-01-11(11:57:44 +0000)

### Changes

- Add LDFLAGS and CFLAGS allowing to search libraries under /opt/prplos

## Release v1.8.1 - 2021-12-28(13:22:53 +0000)

### Fixes

- Clean ressources when cwmpd cant connect

## Release v1.8.0 - 2021-12-14(15:04:10 +0000)

### New

- Wait for required objects before starting cwmpd

## Release v1.7.0 - 2021-12-14(13:19:42 +0000)

### New

- Downgrade to libwebsockets3

## Release v1.6.0 - 2021-11-24(13:13:42 +0000)

### New

- Add support for ACS Address Family

## Release v1.5.1 - 2021-11-19(10:57:11 +0000)

### Fixes

- properly handle the connection with ACSIP list

## Release v1.5.0 - 2021-11-16(14:52:24 +0000)

### New

- support for ACS evnt subscription

## Release v1.4.1 - 2021-11-08(09:33:14 +0000)

### Fixes

- enable persistent settings and use ODL persistent storage

## Release v1.4.0 - 2021-10-29(12:47:34 +0000)

### New

- use DNS Service to resolve ACS Server IP
- properly handle root parameter

## Release v1.3.2 - 2021-10-22(15:45:00 +0000)

### Fixes

- fix ld issue for g++ compilation
- issue: PCF-361 fix g++ build for opensource CI

## Release v1.3.1 - 2021-10-20(10:57:54 +0000)

### Fixes

- Fix SAHTRACE for cwmp-plugin

## Release v1.3.0 - 2021-10-19(13:38:14 +0000)

### New

- add event subscription

## Release v1.2.0 - 2021-10-08(07:56:43 +0000)

### New

- Impl ACS/RPCs Reboot/FactoryReset

## Release v1.1.0 - 2021-10-06(11:43:40 +0000)

### New

- [CWMPD] Add connection security

### Fixes

- FIX GPN with Device. and nextlevel true
- GPN on empty template should return empty
- gitlab CI build componenet error

## Release v1.0.3 - 2021-09-23(09:40:00 +0000)

### Fixes

- Fix warnings cwmpd

### Other

- unit test for dmadapter, cwmp_plugin

## Release v1.0.2 - 2021-09-17(12:05:47 +0000)

### Fixes

- Fix linking issue with libhttpparser

### Other

- Correct URL in baf.yml

## Release v1.0.1 - 2021-09-16(08:44:34 +0000)

### Other

- M3 delivery

