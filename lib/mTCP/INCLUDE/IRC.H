
/*

   mTCP Irc.H
   Copyright (C) 2008-2020 Michael B. Brutman (mbbrutman@gmail.com)
   mTCP web page: http://www.brutman.com/mTCP


   This file is part of mTCP.

   mTCP is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   mTCP is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with mTCP.  If not, see <http://www.gnu.org/licenses/>.


   Description: IRC defines

   Changes:

   2011-05-27: Initial release as open source software

*/


#ifndef _IRC_H
#define _IRC_H



// RFC 2812 says ..
//
// IRCNICK_MAX_LEN (9+1)
// IRCUSER_MAX_LEN is not specified
// IRCREALNAME_MAX_LEN is not specified
// IRCCHANNEL_MAX_LEN (50+1)
// IRCHOSTNAME_MAX_LEN (63+1)
//
// The +1s are for the trailing NULL byte.
//
// We are not totally compliant, but I think 25 chars for nicks
// and channels is more than enough.

#define IRCNICK_MAX_LEN     (50+1)
#define IRCUSER_MAX_LEN     (50+1)
#define IRCREALNAME_MAX_LEN (50+1)
#define IRCPASS_MAX_LEN     (50+1)
#define IRCCHANNEL_MAX_LEN  (50+1)
#define IRCHOSTNAME_MAX_LEN (64)


#define IRC_MSG_MAX_LEN (512)


#define IRC_RPL_WELCOME              (001)
#define IRC_RPL_YOURHOST             (002)
#define IRC_RPL_CREATED              (003)
#define IRC_RPL_MYINFO               (004)
#define IRC_RPL_ISUPPORT             (005)

#define IRC_RPL_UMODEIS              (221)

#define IRC_STATSDLINE               (250)
#define IRC_RPL_LUSERCLIENT          (251)
#define IRC_RPL_LUSEROP              (252)
#define IRC_RPL_LUSERUNKNOWN         (253)
#define IRC_RPL_LUSERCHANNELS        (254)
#define IRC_RPL_LUSERME              (255)

#define IRC_RPL_LOCALUSERS           (265)
#define IRC_RPL_GLOBALUSERS          (266)

#define IRC_RPL_AWAY                 (301)

#define IRC_RPL_CHANNEL_URL          (328)

#define IRC_RPL_NOTOPIC              (331)
#define IRC_RPL_TOPIC                (332)
#define IRC_RPL_TOPICWHOTIME         (333)

#define IRC_RPL_NAMREPLY             (353)
#define IRC_RPL_ENDOFNAMES           (366)

#define IRC_RPL_INFO                 (371)
#define IRC_RPL_MOTD                 (372)
#define IRC_RPL_INFOSTART            (373)
#define IRC_RPL_ENDOFINFO            (374)
#define IRC_RPL_MOTDSTART            (375)
#define IRC_RPL_ENDOFMOTD            (376)


#define IRC_ERR_NO_SUCH_NICK         (401)
#define IRC_ERR_NO_SUCH_SERVER       (402) 
#define IRC_ERR_NO_SUCH_CHANNEL      (403)
#define IRC_ERR_CANNOT_SEND_TO_CHAN  (404)
#define IRC_ERR_TOO_MANY_CHANNELS    (405)
#define IRC_ERR_WAS_NO_SUCHNICK      (406)
#define IRC_ERR_TOO_MANY_TARGETS     (407)
#define IRC_ERR_NO_SUCH_SERVICE      (408)
#define IRC_ERR_NO_ORIGIN            (409)
#define IRC_ERR_NO_RECIPIENT         (411)
#define IRC_ERR_NO_TEXT_TO_SEND      (412)
#define IRC_ERR_NO_TOP_LEVEL         (413)
#define IRC_ERR_WILD_TOP_LEVEL       (414)
#define IRC_ERR_BAD_MASK             (415)
#define IRC_ERR_UNKNOWN_COMMAND      (421)
#define IRC_ERR_NO_MOTD              (422)
#define IRC_ERR_NO_ADMIN_INFO        (423)
#define IRC_ERR_FILE_ERROR           (424)
#define IRC_ERR_NO_NICKNAME_GIVEN    (431)
#define IRC_ERR_ERRONEOUS_NICKNAME   (432)
#define IRC_ERR_NICKNAME_IN_USE      (433)
#define IRC_ERR_NICK_COLLISION       (436)
#define IRC_ERR_UNAVAIL_RESOURCE     (437)
#define IRC_ERR_WAITASEC             (439)
#define IRC_ERR_USER_NOT_IN_CHANNEL  (441)
#define IRC_ERR_NOT_ON_CHANNEL       (442)
#define IRC_ERR_USER_ON_CHANNEL      (443)
#define IRC_ERR_NO_LOGIN             (444) 
#define IRC_ERR_SUMMON_DISABLED      (445)
#define IRC_ERR_USERS_DISABLED       (446)
#define IRC_ERR_NOT_REGISTERED       (451)
#define IRC_ERR_NEED_MORE_PARAMS     (461)
#define IRC_ERR_ALREADY_REGISTERED   (462)
#define IRC_ERR_NO_PERM_FOR_HOST     (463)
#define IRC_ERR_PASSWD_MISMATCH      (464)
#define IRC_ERR_YOURE_BANNED_CREEP   (465)
#define IRC_ERR_YOU_WILL_BE_BANNED   (466)
#define IRC_ERR_KEY_SET              (467)
#define IRC_ERR_CHANNEL_IS_FULL      (471)
#define IRC_ERR_UNKNOWN_MODE         (472)
#define IRC_ERR_INVITE_ONLY_CHAN     (473)
#define IRC_ERR_BANNED_FROM_CHAN     (474)
#define IRC_ERR_BAD_CHANNEL_KEY      (475)
#define IRC_ERR_BAD_CHANNEL_MASK     (476)
#define IRC_ERR_NO_CHANNEL_MODES     (477)
#define IRC_ERR_BAN_LIST_FULL        (478)
#define IRC_ERR_NO_PRIVILEGES        (481)
#define IRC_ERR_CHANOP_PRIVS_NEEDED  (482)
#define IRC_ERR_CANT_KILL_SERVER     (483)
#define IRC_ERR_RESTRICTED           (484)
#define IRC_ERR_UNIQ_OP_PRIVS_NEEDED (485)
#define IRC_ERR_NO_OPER_HOST         (491)
#define IRC_ERR_UMODE_UNKNOWN_FLAG   (501)
#define IRC_ERR_USERS_DONT_MATCH     (502)



#endif
