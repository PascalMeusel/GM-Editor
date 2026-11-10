#pragma once

#include <string>
#include <vector>

struct CalendarMonthModel
{
    std::string name;
    int days = 30;
};

struct InWorldDateTime
{
    int year = 1;
    int month = 1;
    int day = 1;
    int hour = 0;
    int minute = 0;
};

struct CampaignCalendarModel
{
    std::string name = "Gregorian Calendar";
    std::vector<CalendarMonthModel> months;
    std::vector<std::string> weekdays;
    int hoursPerDay = 24;
    int daylightHours = 12;
    int nightHours = 12;
    int dayStartsAtHour = 6;
};

struct CampaignClockModel
{
    CampaignCalendarModel calendar;
    bool useSystemTime = true;
    bool running = true;
    double multiplier = 1.0;
    long long anchorAbsoluteMinutes = 0;
    std::string anchorUtc;
};
