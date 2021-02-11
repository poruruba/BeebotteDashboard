'use strict';

//var vConsole = new VConsole();

const BBT_APIKEY = '【BeebotteのAPIキー】';

var vue_options = {
    el: "#top",
    data: {
        progress_title: '', // for progress-dialog

        elements: [],
        view_styles: [],
        view_data: [],
        bbt_value: [],
        data_name: '',
        image_path: null,
        data_new: {
            type: 'string',
        },
        data_new_color: '#000000'
    },
    computed: {
    },
    methods: {
        toRGB: function(color){
            return '#' + ('000000' + Number(color).toString(16)).slice(-6);
        },
        fromRGB: function(rgb){
            return parseInt(this.data_new_color.substr(1), 16);
        },
        data_image_update: function(){
            this.bbt.disconnect();
            this.bbt.connect();

            this.view_styles = [];
            this.view_data = [];
            for(var i = 0 ; i < this.elements.length ; i++ ){
                var element = JSON.parse(JSON.stringify(this.elements[i]));
                var style = {
                    position: 'absolute',
                    top: element.y + 'px',
                    left: element.x + 'px',
                    'font-size': element.size + 'px',
                    color: this.toRGB(element.color),
                };
                if( element.align == 'center')
                    style.transform = 'translateX(-50%)';
                else if( element.align == 'right')
                    style.transform = 'translateX(-100%)';

                this.view_styles.push(style);
                this.view_data.push(element);

                if( element.type.startsWith('bbt_')){
                    var topic = element.topic.split('/');
                    this.bbt.subscribe( {channel: 'private-' + topic[0], resource: topic[1]}, (message) =>{
                        var channel = message.channel;
                        if( channel.startsWith('private-') )
                            channel = message.channel.substr(8);
                        var index = this.elements.findIndex(item => item.topic == (channel + '/' + message.resource) );
                        if( index >= 0 )
                           vue.$set(this.bbt_value, index, message.data);
                    });
                }
                
            }
        },
        data_close: function(){
            if( this.btt )
                this.bbt.disconnect();
            this.view_data = [];
            this.view_styles = [];
            this.elements = [];
            this.data_name = '';
            this.image_path = null;
        },
        data_load: async function(){
            var json = await do_post('/beebotte-get', { name: this.data_name });
            this.elements = json.data;
            this.image_path = json.path + 'background.png';
            this.data_image_update();
        },
        data_save: async function(){
            if( !this.data_name )
                return;

            var param = {
                name: this.data_name,
                data: this.elements
            };
            var json = await do_post('/beebotte-update', param);
            alert('更新しました。');
        },
        call_data_delete: function(index){
            if( !confirm('本当に削除しますか？') )
                return;

            this.elements.splice(index, 1);
        },
        call_data_update: function(index){
            this.data_new = this.elements[index];
            this.data_new_color = this.toRGB(this.data_new.color);
            this.data_index = index;
            this.dialog_open("#data_update");
        },
        do_data_update: async function(){
            this.data_new.color = this.fromRGB(this.data_new_color);
            this.$set(this.elements, this.data_index, this.data_new);

            this.dialog_close('#data_update');
        },
        call_data_new: function(){
            this.data_new = {
                type: 'string',
            };
            this.data_new_color = "#000000";
            this.dialog_open('#data_new');
        },
        do_data_new: function(){
            this.data_new.color = this.fromRGB(this.data_new_color);
            this.elements.push(this.data_new);

            this.dialog_close('#data_new');
        }
    },
    created: function(){
    },
    mounted: function(){
        proc_load();

        this.bbt = new BBT(BBT_APIKEY, { auth_endpoint: '/beebotte-auth' });
    }
};
vue_add_methods(vue_options, methods_bootstrap);
vue_add_components(vue_options, components_bootstrap);
var vue = new Vue( vue_options );

function do_get_data(url) {
    return fetch(url, {
        method: 'GET',
    })
    .then((response) => {
        if (!response.ok)
            throw 'status is not 200';
        return response.json();
    });
}

function do_post(url, body) {
    const headers = new Headers({ "Content-Type": "application/json; charset=utf-8" });

    return fetch(url, {
        method: 'POST',
        body: JSON.stringify(body),
        headers: headers
    })
    .then((response) => {
        if (!response.ok)
            throw 'status is not 200';
        return response.json();
    });
}
