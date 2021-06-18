%define {
/**
  * Datamodel definition written using TR181 spec Issue 2 Amendment 14 (November 2020)
  */

/**
  * This object contains parameters relating to the CPE's association with an ACS.
  */
  %persistent object ManagementServer {
    /** Enables and disables the CPE's support for CWMP.
      * false means that CWMP support in the CPE is disabled, in which case the device MUST NOT send any Inform messages to the ACS or accept any Connection Request notifications from the ACS.
      * true means that CWMP support on the CPE is enabled.
      * The subscriber can re-enable the CPE's CWMP support either by performing a factory reset or by using a LAN-side protocol to change the value of this parameter back to true.
      * The factory default value MUST be true.
      * @version 2.0
      */
      persistent bool EnableCWMP=1;

    /**
      * The [URL] for the CPE to connect to the ACS using the CPE WAN Management Protocol.
      * This parameter MUST be in the form of a valid HTTP or HTTPS URL.
      * The host portion of this URL is used by the CPE for validating the ACS certificate when using SSL or TLS.
      * Note that on a factory reset of the CPE, the value of this parameter might be reset to its factory value.
      * If an ACS modifies the value of this parameter, it SHOULD be prepared to accommodate the situation that the original value is restored as the result of a factory reset.
      * @version 2.0
      */
      persistent string URL {
      default "http://acs-download.qacafe.com";
      }

    /**
      * Username used to authenticate the CPE when making a connection to the ACS using the CPE WAN Management Protocol.
      * This username is used only for HTTP-based authentication of the CPE.
      * Note that on a factory reset of the CPE, the value of this parameter might be reset to its factory value. If an ACS modifies the value of this parameter, it SHOULD be prepared to accommodate the situation that the original value is restored as the result of a factory reset.
      * @version 2.0
      */
      persistent string Username {
        constraint maxvalue 256;
        default "cdrouter";
      }

    /**
      * Password used to authenticate the CPE when making a connection to the ACS using the CPE WAN Management Protocol.
      * This password is used only for HTTP-based authentication of the CPE.
      * Note that on a factory reset of the CPE, the value of this parameter might be reset to its factory value. If an ACS modifies the value of this parameter, it SHOULD be prepared to accommodate the situation that the original value is restored as the result of a factory reset.
      * When read, this parameter returns an empty string, regardless of the actual value.
      * @version 2.0
      */
      persistent string Password {
        constraint maxvalue 256;
        default "cdrouter";
      }

    /**
      * Whether or not the CPE MUST periodically send CPE information to the ACS using the Inform method call.
      * @version 2.0
      */
      persistent bool PeriodicInformEnable=1;

    /**
      * The duration in seconds of the interval for which the CPE MUST attempt to connect with the ACS and call the Inform method if PeriodicInformEnable is True.
      * @version 2.0
      */
      persistent uint32 PeriodicInformInterval {
        constraint minvalue 1;
        default 432000;
      }

    /**
      * An absolute time reference in UTC to determine when the CPE will initiate the periodic Inform method calls. Each Inform call MUST occur at this reference time plus or minus an integer multiple of the PeriodicInformInterval.
      * PeriodicInformTime is used only to set the "phase" of the periodic Informs. The actual value of PeriodicInformTime can be arbitrarily far into the past or future.
      * For example, if PeriodicInformInterval is 86400 (a day) and if PeriodicInformTime is set to UTC midnight on some day (in the past, present, or future) then periodic Informs will occur every day at UTC midnight. These MUST begin on the very next midnight, even if PeriodicInformTime refers to a day in the future.
      * The Unknown Time value defined in section 2.2 indicates that no particular time reference is specified. That is, the CPE MAY locally choose the time reference, and needs only to adhere to the specified PeriodicInformInterval.
      * If absolute time is not available to the CPE, its periodic Inform behavior MUST be the same as if the PeriodicInformTime parameter was set to the Unknown Time value.
      * @version 2.9
      */
      persistent datetime PeriodicInformTime;

    /**
      * Indicates support of instance wildcards to the ACS
      * @version V9.2
      */
      persistent bool InstanceWildcardsSupported=1;

    /**
      * ParameterKey provides the ACS a reliable and extensible means to track changes made by the ACS. The value of ParameterKey MUST be equal to the value of the ParameterKey argument from the most recent successful SetParameterValues, AddObject, or DeleteObject method call from the ACS.
      * The CPE MUST set ParameterKey to the value specified in the corresponding method arguments if and only if the method completes successfully and no fault response is generated. If a method call does not complete successfully (implying that the changes requested in the method did not take effect), the value of ParameterKey MUST NOT be modified.
      * The CPE MUST only modify the value of ParameterKey as a result of SetParameterValues, AddObject, DeleteObject, or due to a factory reset. On factory reset, the
      * value of ParameterKey MUST be set to empty.
      * @version 2.0
      */
      persistent string ParameterKey;

    /**
      * HTTP URL, as defined in [8], for an ACS to make a Connection Request notification to the CPE.
      * In the form:
      * http://host:port/path
      * The "host" portion of the URL MAY be the IP address for the management interface of the CPE in lieu of a host name.
      * Note: If the host portion of the URL is a literal IPv6 address then it MUST be enclosed in square brackets (see [Section 3.2.2/RFC3986]).
      * @version 2.0
      */
      read-only string ConnectionRequestURL;

    /**
      * Username used to authenticate an ACS making a Connection Request to the CPE.
      * @version 2.0
      */
      persistent string ConnectionRequestUsername {
          constraint maxvalue 256;
          default "acs";
      }

    /**
      * Password used to authenticate an ACS making a Connection Request to the CPE.
      * When read, this parameter returns an empty string, regardless of the actual value.
      * @version 2.0
      */
      persistent string ConnectionRequestPassword {
          constraint maxvalue 256;
          default "acs";
    }

    /**
      * Indicates whether or not the ACS will manage upgrades for the CPE. If True, the CPE SHOULD NOT use other means other than the ACS to seek out available upgrades. If False, the CPE MAY use other means for this purpose.
      * Note that an autonomous upgrade (reported via an "10 AUTONOMOUS TRANSFER COMPLETE" Inform Event code) SHOULD be regarded as a managed upgade if it is performed according to ACS-specified policy.
      * @version 2.0
      */
      persistent bool UpgradesManaged=1;

    /**
      * This parameter is used to control throttling of active notifications sent by the CPE to the ACS. It defines the minimum number of seconds that the CPE MUST wait since the end of the last session with the ACS before establishing a new session for the purpose of delivering an active notification.
      * In other words, if CPE needs to establish a new session with the ACS for the sole purpose of delivering an active notification, it MUST delay establishing such a session as needed to ensure that the minimum time since the last session completion has been met.
      * The time is counted since the last successfully completed session, regardless of whether or not it was used for active notifications or other purposes. However, if connection to the ACS is established for purposes other than just delivering active notifications, including for the purpose of retrying a failed session, such connection MUST NOT be delayed based on this parameter value, and the pending active notifications MUSTbe communicated during that connection.
      * The time of the last session completion does not need to be tracked across reboots.
      * @version 2.0
      */
      persistent uint32 DefaultActiveNotificationThrottle;

    /**
      * Indicates whether or not the Alias-Based Addressing Mechanism is supported.
      * A true value indicates that the CPE supports the Alias-Based Addressing Mechanism, as defined in [Section 3.6.1/TR-069] and described in [Appendix II/TR-069].     * @version 6.0
      * @version 2.3
      */
      read-only bool AliasBasedAddressing = true;

    /**
      * Instance identification mode as defined in [Section 3.6.1/TR-069]. When AliasBasedAddressing is true, InstanceMode is used by the ACS to control whether the CPE will use Instance Numbers or Instance Aliases in returned Path Names. Enumeration of:
      * - InstanceNumber
      * - InstanceAlias 
      * This parameter is REQUIRED for any CPE supporting Alias-Based Addressing.
      * The factory default value MUST be InstanceNumber.
      * @version 2.3
      */
      read-only string InstanceMode {
        constraint enum ["InstanceNumber","InstanceAlias"];
        default "InstanceNumber";
      }

    /**
      * Enable or disable the Auto-Create Instance Mechanism. When AliasBasedAddressing is true, AutoCreateInstances indicates whether or not the CPE will automatically create instances while processing a SetParameterValues RPC (as defined in [A.3.2.1/TR-069]).
      * - A true value indicates that the CPE will perform auto-creation of instances when the Alias-Based Addressing Mechanism is used in SetParameterValues RPC.
      * - A false value indicates that the CPE will not create new object instances. Instead, it will reject the setting of parameters in unrecognized instances and respond with a fault code. 
      * This parameter is REQUIRED for any CPE supporting Alias-Based Addressing.
      * The factory default value MUST be false.
      * @version 2.3
      */
      read-only bool AutoCreateInstances = true;
    /**
      * Load and save functions for ambiorix consistancy
      */
          void load(%in bool reset = true);
          void save();
  }
}

%populate {
}
