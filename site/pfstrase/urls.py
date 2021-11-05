"""pfstrase URL Configuration

The `urlpatterns` list routes URLs to views. For more information please see:
    https://docs.djangoproject.com/en/2.1/topics/http/urls/
Examples:
Function views
    1. Add an import:  from my_app import views
    2. Add a URL to urlpatterns:  path('', views.home, name='home')
Class-based views
    1. Add an import:  from other_app.views import Home
    2. Add a URL to urlpatterns:  path('', Home.as_view(), name='home')
Including another URLconf
    1. Import the include() function: from django.urls import include, path
    2. Add a URL to urlpatterns:  path('blog/', include('blog.urls'))
"""
from django.contrib import admin
from django.urls import path
from django.views.static import serve
import pfstrase.settings as settings
from pfs_app.views import home, history, tag_detail

urlpatterns = [
    path('admin/', admin.site.urls),
    path('', home, name='home'),    
    path('history/<slug:host>/', history, name='history'),    
    path('tag_detail/', tag_detail, name='tag_detail'),    
    path(r'media/<path>', serve, {'document_root': settings.MEDIA_ROOT}, 
         name = "media"), 
]
