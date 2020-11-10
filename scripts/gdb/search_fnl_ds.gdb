define search_fnl_ds_byname
set var $fnl=$arg0
set var $first=(struct data_state_rw *)$fnl->data_state_list.next
set var $entry=$first
while $entry
	if $entry->base.ref_type == 1
		set var $vn=(struct var_node *)$entry->base.ref_base
		if $_streq($arg1,$vn->name)
			p $entry
			end
		end
	set var $entry=(struct data_state_rw *)$entry->base.sibling.next
	end
end
