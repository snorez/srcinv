define search_global_ds_rw_byname
set var $first=(struct data_state_rw *)si->global_data_rw_states.next
set var $entry=$first
while $entry
	if $entry->base.ref_type == 1
		set var $vn=(struct var_node *)$entry->base.ref_base
		if $_streq($arg0,$vn->name)
			p $entry
			end
		end
	set var $entry=(struct data_state_rw *)$entry->base.sibling.next
	end
end
