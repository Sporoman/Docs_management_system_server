#include "commands.h"

namespace Commands
{
    const char* getNameCommand(Command commandNumber)
    {
        switch (commandNumber)
        {
            case Command::identity:          return "identity";
            case Command::quit:              return "quit";
            case Command::userInfo:          return "userInfo";
            case Command::getDocs:           return "getDocs";
            case Command::getFavDocs:        return "getFavDocs";
            case Command::addFavoriteDoc:    return "addFavoriteDoc";
            case Command::deleteFavoriteDoc: return "deleteFavoriteDoc";
            case Command::sendDocInfo:       return "sendDocInfo";
            case Command::sendDocFullInfo:   return "sendDocFullInfo";
            case Command::sendDocToClient:   return "sendDocToClient";
            case Command::sendDocToServer:   return "sendDocToServer";
            case Command::addNewUser:        return "addNewUser";
            case Command::getRolesAndLevels: return "getRolesAndLevels";
            case Command::searchUserForEdit: return "searchUserForEdit";
            case Command::searchUserForShow: return "searchUserForShow";
            case Command::editUser:          return "editUser";
            case Command::editUserInfo:      return "editUserInfo";
            case Command::setUserStatus:     return "setUserStatus";
            case Command::editDocInfo:       return "editDocInfo";
            case Command::deleteDoc:         return "deleteDoc";
            case Command::getStatistics:     return "getStatistics";

            default: case Command::error:    return "error";
        }
    }
}

