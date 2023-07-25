import Head from 'next/head';
import styles from '../styles/Home.module.css';
import Header from '../components/header';
import { TextField, Button, Avatar } from '@mui/material';
import { useEffect, useState } from 'react';

function genRandomName() {
  const number = Math.floor(Math.random() * 999999);
  return `user-${number}`
}


export default function Home() {
  const [userName, setUserName] = useState('');
  useEffect(() => {
    setUserName(localStorage.getItem('servertech_username') || genRandomName());
  })
  return (
    <>
      <Head>
        <title>BoostServerTech chat</title>
        <link rel="icon" href="/favicon.ico" />
      </Head>
      <div className="flex flex-col">
        <Header />
        <div className="flex-1 flex border-">
          <div className='flex-1 flex flex-col'>
            <div className='flex'>
              <Avatar>A</Avatar>
              <div className='flex-1 flex flex-col'>
                <p className='m-0'>Room name</p>
                <p className='m-0'>Room last message</p>
              </div>
            </div>
          </div>
          <div className='flex-[3] flex flex-col' style={{ backgroundColor: 'var(--boost-light-grey)' }}>
            <span>Messages</span>
          </div>
        </div>
      </div>
    </>
  )
}
