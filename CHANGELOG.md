# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]


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

