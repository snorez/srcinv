define search_global_ds_base_byname
set var $first=(struct data_state_base *)si->global_data_states.next
set var $entry=$first
while $entry
	if $entry->ref_type == 1
		set var $vn=(struct var_node *)$entry->ref_base
		if $_streq($arg0,$vn->name)
			p $entry
			end
		end
	set var $entry=(struct data_state_base *)$entry->sibling.next
	end
end
