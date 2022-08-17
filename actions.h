#ifndef ACTIONS_H
#define ACTIONS_H

namespace Actions
{
    enum ActionsId
    {
        download_doc        = 1,
        add_fav_doc         = 2,
        remove_fav_doc      = 3,
        update_info_doc     = 4,
        update_info_profile = 5,
        update_info_account = 6,
        create_account      = 7,
        delete_account      = 8,
        delete_doc          = 9,
        upload_doc          = 10,
        set_active_true     = 11,
        set_active_false    = 12
    };
}

#endif // ACTIONS_H
