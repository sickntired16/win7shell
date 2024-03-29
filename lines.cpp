#include <string>
#include <vector>
#include <sstream>
#include <Windows.h>

#include "gen_win7shell.h"
#include "resource.h"
#include "api.h"
#include "wa_ipc.h"
#include "lines.h"

lines::lines(const sSettings &Settings, MetaData &Metadata, const HWND WinampWnd) :
m_settings(Settings),
m_metadata(Metadata),
m_hwnd(WinampWnd)
{
    
}

inline std::wstring SecToTime(int sec)
{	
    int ZH = sec / 3600;
    int ZM = sec / 60 - ZH * 60;
    int ZS = sec - (ZH * 3600 + ZM * 60);

    std::wstringstream ss;
    if (ZH != 0)
    {
        if (ZH < 10)
            ss << L"0" << ZH << L":";
        else
            ss << ZH << L":";
    }

    if (ZM < 10)
        ss << L"0" << ZM << L":";
    else
        ss << ZM << L":";

    if (ZS < 10)
        ss << L"0" << ZS;
    else
        ss << ZS;

    return ss.str();
}

inline std::wstring lines::MetaWord(const std::wstring &word)
{
    if (word == __T("%curtime%"))
    {
        int res = SendMessage(m_hwnd,WM_WA_IPC,0,IPC_GETOUTPUTTIME);
        if (res == -1)
            return __T("~");
        return SecToTime(res/1000);
    }

    if (word == __T("%timeleft%"))
    {
        int cur = SendMessage(m_hwnd,WM_WA_IPC,0,IPC_GETOUTPUTTIME);
        int tot = SendMessage(m_hwnd,WM_WA_IPC,1,IPC_GETOUTPUTTIME);
        if (cur == -1 || tot == -1)
            return __T("~");
        return SecToTime(tot - cur / 1000);
    }

    if (word == __T("%totaltime%"))
    {
        int res = SendMessage(m_hwnd,WM_WA_IPC,1,IPC_GETOUTPUTTIME);
        if (res == -1)
            return __T("");
        return SecToTime(res);
    }

    if (word == __T("%kbps%"))
    {
        int inf=SendMessage(m_hwnd,WM_WA_IPC,1,IPC_GETINFO);
        if (inf == NULL)
            return std::wstring(__T(""));
        std::wstringstream w;
        w << inf;
        return w.str();
    }

    if (word == __T("%khz%"))
    {
        int inf=SendMessage(m_hwnd,WM_WA_IPC,0,IPC_GETINFO);
        if (inf == NULL)
            return std::wstring(__T(""));
        std::wstringstream w;
        w << inf;
        return w.str();
    }

    if (word == __T("%c%"))
        return __T("‡center‡");

    if (word == __T("%l%"))
        return __T("‡largefont‡");

    if (word == __T("%f%"))
        return __T("‡forceleft‡");

    if (word == __T("%s%"))
        return __T("‡shadow‡");

    if (word == __T("%b%"))
        return __T("‡darkbox‡");

    if (word == __T("%d%"))
        return __T("‡dontscroll‡");

    if (word == __T("%volume%"))
    {
        std::wstringstream w;
        w << (m_settings.play_volume * 100) /255;
        return w.str();
    }

    if (word == __T("%shuffle%"))
    {
        static wchar_t tmp[8];
        return WASABI_API_LNGSTRINGW_BUF((SendMessage(m_hwnd,WM_WA_IPC,0,IPC_GET_SHUFFLE)?IDS_ON:IDS_OFF),tmp,8);
    }

    if (word == __T("%repeat%"))
    {
        static wchar_t tmp[8];
        return WASABI_API_LNGSTRINGW_BUF((SendMessage(m_hwnd,WM_WA_IPC,0,IPC_GET_REPEAT)?IDS_ON:IDS_OFF),tmp,8);
    }

    if (word == __T("%curpl%"))
    {
        std::wstringstream w;
        w << (SendMessage(m_hwnd,WM_WA_IPC,0,IPC_GETLISTLENGTH)? m_settings.play_playlistpos+1 : 0);
        return w.str();
    }

    if (word == __T("%totalpl%"))
    {
        std::wstringstream w;
        w << SendMessage(m_hwnd,WM_WA_IPC,0,IPC_GETLISTLENGTH);
        return w.str();
    }

    if (word == __T("%rating1%") || word == __T("%rating2%"))
    {
        std::wstringstream w;
        int x = SendMessage(m_hwnd,WM_WA_IPC,0,IPC_GETRATING);
        if (word == __T("%rating1%"))
            w << x;
        else
        {
            for (int i=0; i < x; i++)
                w << L"\u2605";
            for (int i=0; i < 5-x; i++)
                w << L"\u2606"/*" \u25CF"*/;
        }
        return w.str();
    }

    return m_metadata.getMetadata(word.substr(1, word.length()-2));
}

void lines::Parse()
{
    m_linesettings.clear();
    m_texts.clear();

    // parse all text
    {
        std::wstring::size_type pos = 0;
        do 
        {
            std::wstring::size_type pos_2 = m_settings.Text.find_first_of(NEWLINE, pos);
            if (pos_2 == std::wstring::npos)
            {
                m_texts.push_back(m_settings.Text.substr(pos));
                break;
            }
            m_texts.push_back(m_settings.Text.substr(pos, pos_2-pos));
            pos = pos_2 + 1;
    
        } while (pos != std::wstring::npos);
    }

    // replace text formatting tags

    std::wstring::size_type pos = 0, open = std::wstring::npos;
    std::wstring metaword;

    for (std::size_t index = 0; index != m_texts.size(); ++index)
    {
        linesettings current_line_settings = 
        {
            false, 
            false, 
            false, 
            false,
            false,
            false
        };
    
        pos = 0;
        do
        {
            pos = m_texts[index].find_first_of(L'%', pos);

            if (pos != std::wstring::npos)
            {
                if (pos != 0 && m_texts[index][pos-1] == L'\\')
                {
                    m_texts[index].erase(pos-1, 1);
                    continue;
                }

                if (open != std::wstring::npos)
                {
                    metaword = MetaWord(m_texts[index].substr(open, pos-open+1));
                    if (metaword == __T("‡center‡"))
                    {
                        metaword = __T("");
                        current_line_settings.center = true;
                    }
                    if (metaword == __T("‡largefont‡"))
                    {
                        metaword = __T("");
                        current_line_settings.largefont = true;
                    }
                    if (metaword == __T("‡forceleft‡"))
                    {
                        metaword = __T("");
                        current_line_settings.forceleft = true;
                    }
                    if (metaword == __T("‡shadow‡"))
                    {
                        metaword = __T("");
                        current_line_settings.shadow = true;
                    }
                    if (metaword == __T("‡darkbox‡"))
                    {
                        metaword = __T("");
                        current_line_settings.darkbox = true;
                    }
                    if (metaword == __T("‡dontscroll‡"))
                    {
                        metaword = __T("");
                        current_line_settings.dontscroll = true;
                    }
    
                    m_texts[index].replace(open, pos-open+1, metaword);
                    open = std::wstring::npos;
                    pos = std::wstring::npos;
                }
                else
                    open = pos;
                pos++;
            }
        }
        while (pos!=std::wstring::npos);

        m_linesettings.push_back(current_line_settings);
    }
}
