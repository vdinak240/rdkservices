/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2019 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#include "Telemetry.h"

#include "utils.h"

// Methods
#define TELEMETRY_METHOD_GET_AVAILABLE_REPORT_PROFILES "getAvailableReportProfiles"
#define TELEMETRY_METHOD_SET_REPORT_PROFILE_STATUS"setReportProfileStatus"
#define TELEMETRY_METHOD_LOG_APPLICATION_EVENT "logApplicationEvent"

#define RFC_CALLERID "Telemetry"
#define RFC_REPORT_PROFILES "Device.X_RDKCENTRAL-COM_T2.ReportProfiles"
#define RFC_REPORT_PROFILE_ENABLE "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry."

#define DEFAULT_PROFILES_FILE "/etc/t2profiles/default.json"

namespace WPEFramework
{
    namespace Plugin
    {
        SERVICE_REGISTRATION(Telemetry, 1, 0);

        Telemetry* Telemetry::_instance = nullptr;

        Telemetry::Telemetry()
        : AbstractPlugin()
        {
            Telemetry::_instance = this;

            registerMethod(TELEMETRY_METHOD_GET_AVAILABLE_REPORT_PROFILES, &Telemetry::getAvailableReportProfiles, this);
            registerMethod(TELEMETRY_METHOD_SET_REPORT_PROFILE_STATUS, &Telemetry::setReportProfileStatus, this);
            registerMethod(TELEMETRY_METHOD_LOG_APPLICATION_EVENT, &Telemetry::logApplicationEvent, this);
        }

        Telemetry::~Telemetry()
        {

        }

        void Telemetry::Deinitialize(PluginHost::IShell* /* service */)
        {
            Telemetry::_instance = nullptr;
        }

        uint32_t Telemetry::getAvailableReportProfiles(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            JsonArray profiles;
            getProfiles(profiles);

            JsonArray profilesRes;
            JsonArray::Iterator it = profiles.Elements();
            while(it.Next())
            {
                JsonObject o;
                o["name"] = it.Current().String();
                profilesRes.Add(o);
            }

            response["reportProfiles"] = profilesRes;

            returnResponse(true);
        }

        uint32_t Telemetry::setReportProfileStatus(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            if (parameters.HasLabel("reportProfile") && parameters.HasLabel("status"))
            {
                string status;
                getStringParameter("status", status);

                if (status != "STARTED" && status != "COMPLETE")
                {
                    LOGERR("Only the 'STARTED' or 'COMPLETE' status is allowed");
                    returnResponse(false);
                }

                string reportProfile;
                getStringParameter("reportProfile", reportProfile);

                JsonArray profiles;
                getProfiles(profiles);

                JsonArray::Iterator it = profiles.Elements();
                while(it.Next())
                {
                    std::string profile = it.Current().String();
                    if (profile == reportProfile)
                    {
                        std::string rfc = RFC_REPORT_PROFILE_ENABLE;
                        rfc += profile;
                        rfc += "Report.Enable";

                        WDMP_STATUS wdmpStatus = setRFCParameter(RFC_CALLERID, rfc.c_str(), status == "COMPLETE" ? "true" : "false", WDMP_BOOLEAN);
                        if (WDMP_SUCCESS != wdmpStatus)
                        {
                            LOGERR("Failed to set %s: %d", rfc.c_str(), wdmpStatus);
                            returnResponse(false);
                        }

                        returnResponse(true);
                    }
                }
                LOGERR("Didn't find %s in the list of available profiles", reportProfile.c_str());
                returnResponse(false);
            }
            else
            {
                LOGERR("No 'reportProfile' or 'status' parameter");
                returnResponse(false);
            }

            returnResponse(false);
        }

        uint32_t Telemetry::logApplicationEvent(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            if (parameters.HasLabel("eventName") && parameters.HasLabel("eventValue"))
            {
                string eventName;
                getStringParameter("eventName", eventName);

                string eventValue;
                getStringParameter("eventValue", eventValue);

                LOGT2((char *)eventName.c_str(), (char *)eventValue.c_str());
            }
            else
            {
                LOGERR("No 'eventName' or 'eventValue' parameter");
                returnResponse(false);
            }

            returnResponse(true);
        }

        void Telemetry::getProfiles(JsonArray &profiles)
        {
            profiles.Clear();

            Core::File file;
            file = DEFAULT_PROFILES_FILE;
            file.Open();
            if (file.IsOpen())
            {
                JsonObject defProfilePack;
                defProfilePack.IElement::FromFile(file);

                if (defProfilePack.HasLabel("profiles"))
                {
                    JsonArray defProfiles = defProfilePack["profiles"].Array();

                    JsonArray::Iterator it = defProfiles.Elements();

                    while(it.Next())
                    {
                        JsonObject profile = it.Current().Object();
                        if (profile.HasLabel("name"))
                            profiles.Add(profile["name"]);
                        else
                            LOGERR("Failed to parse profile name from %s", DEFAULT_PROFILES_FILE);
                    }
                }
            }
            LOGERR("Failed to open %s", DEFAULT_PROFILES_FILE);

            file.Close();

            DIR *d = opendir("/opt/.t2persistentfolder/");

            if (NULL != d)
            {
                struct dirent *de;

                while ((de = readdir(d)))
                {
                    if (0 == de->d_name[0])
                        continue;

                    if (0 == strcmp(de->d_name, ".") || 0 == strcmp(de->d_name, ".."))
                        continue;

                    bool exists = false;
                    JsonArray::Iterator it = profiles.Elements();

                    while(it.Next())
                        if (it.Current().String() == de->d_name)
                            exists = true;

                    if (!exists)
                        profiles.Add(de->d_name);
                }

                closedir(d);
            }
        }

    } // namespace Plugin
} // namespace WPEFramework

