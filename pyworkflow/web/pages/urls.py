from django.conf.urls import patterns, include, url
# Uncomment the next two lines to enable the admin:
from django.contrib import admin
admin.autodiscover()
from django.conf import settings

urlpatterns = patterns('',
    (r'^resources/(?P<path>.*)$', 'django.views.static.serve', {'document_root': settings.MEDIA_ROOT}),
    (r'^static/(?P<path>.*)$', 'django.views.static.serve', {'document_root': settings.STATIC_ROOT}),
                
    # Examples:
    # url(r'^pages/', include('pages.foo.urls')),

    # Uncomment the admin/doc line below to enable admin documentation:
    # url(r'^pages/doc/', include('django.contrib.admindocs.urls')),

    # Uncomment the next line to enable the admin:
    url(r'^admin/', include(admin.site.urls)),
    
    #HOST
    url(r'^view_hosts', 'app.views_host.viewHosts', name='view_hosts'),    
    url(r'^update_host', 'app.views_host.updateHost'),
    url(r'^delete host/$', 'app.views_host.deleteHost'),
    url(r'^host_form/$', 'app.views_host.hostForm'),
    
    #PROJECT (CONTENT, RUNTABLE AND GRAPH)
    url(r'^projects/', 'app.views_project.projects'),
    url(r'^create_project/$', 'app.views_project.create_project'),
    url(r'^delete_project/$', 'app.views_project.delete_project'),
    url(r'^project_content/$', 'app.views_project.project_content'),
    url(r'^tree_prot_view/$', 'app.views_project.tree_prot_view'),
    url(r'^run_table_graph/$', 'app.views_project.run_table_graph'),
    url(r'^protocol_info/$', 'app.views_project.protocol_info'),
    url(r'^protocol_status/$', 'app.views_project.protocol_status'),
    url(r'^project_graph/$', 'app.views_project.project_graph'),
    url(r'^update_graph_view/$', 'app.views_project.update_graph_view'),
    url(r'^update_prot_tree/$', 'app.views_project.update_prot_tree'),
    
    #UTILS
    url(r'^render_column/', 'app.views_util.render_column'), # Load images dynamically
    url(r'^get_image_plot/', 'app.views_util.get_image_plot'), # Load plots images dynamically
    url(r'^get_image/', 'app.views_util.get_image'), # Load images dynamically
    url(r'^get_slice/', 'app.views_util.get_slice'), # Load slices dynamically
    url(r'^browse_objects/$', 'app.views_util.browse_objects'), # Browse objects from the database
    url(r'^browse_protocol_class/$', 'app.views_util.browse_protocol_class'), # Browse objects from the database
    url(r'^get_attributes/$', 'app.views_util.get_attributes'), # Get Label and Comment for an Object
    url(r'^set_attributes/$', 'app.views_util.set_attributes'), # Set Label and Comment for an Object


    #PROTOCOL (INCLUDE FORM)
    url(r'^save_protocol/$', 'app.views_protocol.save_protocol'),
    url(r'^protocol/$', 'app.views_protocol.protocol'),
    url(r'^stop_protocol/$', 'app.views_protocol.stop_protocol'),
    url(r'^delete_protocol/$', 'app.views_protocol.delete_protocol'),
    url(r'^form/$', 'app.views_protocol.form'),
    

    #WIZARDS
    url(r'^wizard/$', 'app.em_wizard.wizard'),
    url(r'^get_image_psd/$', 'app.em_wizard.get_image_psd'),
    url(r'^get_image_bandpass/$', 'app.em_wizard.get_image_bandpass'),
    url(r'^get_image_gaussian/$', 'app.em_wizard.get_image_gaussian'),
    url(r'^get_image_filter_spider/$', 'app.em_wizard.get_image_filter_spider'),
    
    #VIEWERS
    url(r'^launch_viewer/$', 'app.em_viewer.launch_viewer'),
    url(r'^viewerElement/$', 'app.em_viewer.viewerElement'),
    
    #SHOWJ
    url(r'^showj/$', 'app.views_showj.showj'), #Load web
    url(r'^showj_pdb/', 'app.views_showj.showj_pdb'), #Load web
    url(r'^save_showj_table/', 'app.views_showj.save_showj_table'), # Save table to session variable dynamically
    url(r'^showVolVisualization/', 'app.views_showj.showVolVisualization'),

    
    url(r'^update_session_table/$', 'app.views_showj.updateSessionTable'),
    
    url(r'^showj_menu/$', 'app.views_showj.showj_menu'), #Load showj web menu
    url(r'^showj_gallery/$', 'app.views_showj.showj_gallery'), #Load showj web gallery
    url(r'^showj_table/$', 'app.views_showj.showj_table'), #Load showj web table
    url(r'^showj_volume_astex/$', 'app.views_showj.showj_volume_astex'), #Load showj web astex visualizer
    url(r'^showj_volume_chimera/$', 'app.views_showj.showj_volume_chimera'), #Load showj web chimera visualizer
    
    #TESTING
    url(r'^testingSSH/', 'app.views_showj.testingSSH'), #Load web
)
