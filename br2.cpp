#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <ctime>

using namespace std;

const string RAW_LOG =
    "/home/saif/Projects/Checkmate_Files/raw.log";

const string CLEAN_LOG =
    "/home/saif/Projects/Checkmate_Files/clean.log";


// Convert timestamp to seconds.
// Example:
// 2026-08-25T00:36:13.244686653+0600
//
// We only need hour/min/sec for the 5-minute comparison.
long long getSeconds(const string& timestamp)
{
    if (timestamp.size() < 19)
        return -1;

    int hour =
        stoi(timestamp.substr(11, 2));

    int minute =
        stoi(timestamp.substr(14, 2));

    int second =
        stoi(timestamp.substr(17, 2));

    return hour * 3600LL +
           minute * 60LL +
           second;
}


vector<string> split(const string& str)
{
    vector<string> result;

    string part;
    stringstream ss(str);

    while (getline(ss, part, '.'))
    {
        if (!part.empty())
            result.push_back(part);
    }

    return result;
}


string getMainDomain(string host)
{
    // Remove trailing dot
    while (!host.empty() && host.back() == '.')
        host.pop_back();

    if (host.empty())
        return "";

    vector<string> parts = split(host);

    /*
        Accept:

        example.com
        www.example.com
        www.www.example.com

        Anything with more than 3 labels
        is ignored.
    */

    if (parts.size() < 2 || parts.size() > 3)
        return "";

    /*
        example.com
    */

    if (parts.size() == 2)
        return parts[0] + "." + parts[1];

    /*
        www.example.com
        abc.example.com

        -> example.com
    */

    return parts[1] + "." + parts[2];
}


int main()
{
    ifstream raw(RAW_LOG);

    if (!raw)
    {
        cerr << "Cannot open raw.log\n";
        return 1;
    }

    ofstream clean(CLEAN_LOG);

    if (!clean)
    {
        cerr << "Cannot create clean.log\n";
        return 1;
    }


    /*
        domain -> last stored timestamp
    */

    map<string, long long> lastSeen;

    string line;

    while (getline(raw, line))
    {
        vector<string> fields;

        string field;
        stringstream ss(line);

        while (getline(ss, field, '\t'))
            fields.push_back(field);

        if (fields.size() < 3)
            continue;


        string timestamp = fields[0];

        string hostname;


        // DNS
        if (fields.size() >= 3 &&
            !fields[2].empty())
        {
            hostname = fields[2];
        }

        // TLS
        else if (fields.size() >= 4 &&
                 !fields[3].empty())
        {
            hostname = fields[3];
        }


        if (hostname.empty())
            continue;


        string domain =
            getMainDomain(hostname);

        if (domain.empty())
            continue;


        long long currentTime =
            getSeconds(timestamp);

        if (currentTime < 0)
            continue;


        /*
            Check when this domain was
            last stored.
        */

        auto it = lastSeen.find(domain);


        if (it != lastSeen.end())
        {
            long long difference =
                currentTime - it->second;


            /*
                Less than 5 minutes:
                don't store it again.
            */

            if (difference < 300)
                continue;
        }


        /*
            Either:

            - first time seen
            OR
            - more than 5 minutes since
              last stored occurrence
        */

        clean << timestamp
              << "\t"
              << domain
              << "\n";


        lastSeen[domain] =
            currentTime;
    }


    raw.close();
    clean.close();


    cout << "Done.\n";
    cout << "Created: "
         << CLEAN_LOG
         << "\n";

    return 0;
}